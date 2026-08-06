// Net_Lockstep — see header for the design and why it is lockstep-with-delay
// rather than the Amiga's state-sync.
//
// This file must not include a platform header. It talks to the network only
// through Net_Socket.h. See the note at the top of Net_Socket.h for what
// happens otherwise.

#include "Net_Lockstep.h"

#include <cstdio>
#include <cstring>

namespace scr {

namespace {

// --- Wire format -----------------------------------------------------------
// Everything is written little-endian byte by byte. No struct is ever memcpy'd
// onto the wire: padding and alignment differ between the three toolchains, and
// a silently-different layout is precisely the class of bug that would present
// as an unreproducible desync.

const uint32_t kMagic = 0x52435353u;    // 'SCSR' little-endian

enum PacketType
{
    Pkt_Hello   = 1,    // joiner -> host
    Pkt_Welcome = 2,    // host -> joiner, carries the agreed configuration
    Pkt_Reject  = 3,    // host -> joiner, with a reason
    Pkt_Input   = 4,    // both ways, every step
    Pkt_Bye     = 5,
    Pkt_Ping    = 6,
    Pkt_Pong    = 7,
    Pkt_Desync  = 8     // "our digests disagree" -- see MarkDesync
};

const int kHeaderBytes = 8;     // magic(4) + type(1) + flags(1) + version(2)

// Comfortably larger than the biggest packet we build: header + base step +
// count + kRedundancy inputs + a digest block.
const int kMaxPacket = 256;

// How long to wait for a WELCOME before giving up.
const double kConnectTimeout = 10.0;

// How long without any packet from the peer before the session is declared
// dead. Generous: a lockstep stall is already visible to the player, and
// killing a session on a brief hiccup is worse than waiting.
const double kSessionTimeout = 8.0;

// Joiner resends HELLO this often until answered — the first one may be the
// packet that opens the host's NAT binding.
const double kHelloInterval = 0.25;

const double kPingInterval = 1.0;

// How often to repeat our inputs while stalled.
//
// Inputs are normally sent only when a new one is submitted, and kRedundancy
// makes that robust *while the race is advancing*. A stall breaks that: if the
// packet carrying our input for the step the peer is waiting on is lost, the
// peer stalls, so it stops submitting, so it stops sending - and we then stall
// waiting on it. Neither side ever sends again and the session deadlocks on a
// single dropped datagram. Repeating while stalled is what breaks the tie.
const double kStallResendInterval = 0.05;

class Writer
{
public:
    Writer(uint8_t* p, int cap) : mP(p), mCap(cap), mN(0) {}

    void U8 (uint8_t v)  { if (mN + 1 > mCap) { mN = mCap + 1; return; } mP[mN++] = v; }
    void U16(uint16_t v) { U8((uint8_t)(v & 0xff)); U8((uint8_t)(v >> 8)); }
    void U32(uint32_t v) { U16((uint16_t)(v & 0xffff)); U16((uint16_t)(v >> 16)); }
    void U64(uint64_t v) { U32((uint32_t)(v & 0xffffffffu)); U32((uint32_t)(v >> 32)); }

    int  Size() const { return mN; }
    bool Ok()   const { return mN <= mCap; }

private:
    uint8_t* mP;
    int      mCap;
    int      mN;
};

class Reader
{
public:
    Reader(const uint8_t* p, int n) : mP(p), mN(n), mI(0), mBad(false) {}

    uint8_t U8()
    {
        if (mI + 1 > mN) { mBad = true; return 0; }
        return mP[mI++];
    }
    uint16_t U16() { uint16_t a = U8(); return (uint16_t)(a | ((uint16_t)U8() << 8)); }
    uint32_t U32() { uint32_t a = U16(); return a | ((uint32_t)U16() << 16); }
    uint64_t U64() { uint64_t a = U32(); return a | ((uint64_t)U32() << 32); }

    // True once a read has run past the end. Every parse checks this before
    // acting on anything it read — a truncated or malicious packet must not be
    // able to steer the session.
    bool Bad() const { return mBad; }

private:
    const uint8_t* mP;
    int            mN;
    int            mI;
    bool           mBad;
};

// double <-> its bit pattern. dt has to match exactly between peers, so it
// crosses the wire as bits; a decimal round-trip could land a ulp away and
// desync everything downstream.
uint64_t BitsOfDouble(double d)
{
    uint64_t b;
    memcpy(&b, &d, sizeof(b));
    return b;
}
double DoubleOfBits(uint64_t b)
{
    double d;
    memcpy(&d, &b, sizeof(d));
    return d;
}

// --- Session ---------------------------------------------------------------

struct InputRing
{
    uint32_t value[kInputRing];
    uint32_t stamp[kInputRing];  // which step each slot holds
    bool     stampValid[kInputRing];

    void Reset()
    {
        memset(value, 0, sizeof(value));
        memset(stamp, 0, sizeof(stamp));
        memset(stampValid, 0, sizeof(stampValid));
    }

    void Set(uint32_t step, uint32_t v)
    {
        int i = (int)(step & (kInputRing - 1));
        value[i]      = v;
        stamp[i]      = step;
        stampValid[i] = true;
    }

    bool Has(uint32_t step) const
    {
        int i = (int)(step & (kInputRing - 1));
        return stampValid[i] && stamp[i] == step;
    }

    uint32_t Get(uint32_t step) const
    {
        int i = (int)(step & (kInputRing - 1));
        return value[i];
    }
};

struct DigestRing
{
    uint32_t step[kInputRing];
    uint64_t hash[kInputRing];
    bool     valid[kInputRing];

    void Reset()
    {
        memset(step, 0, sizeof(step));
        memset(hash, 0, sizeof(hash));
        memset(valid, 0, sizeof(valid));
    }
    void Set(uint32_t s, uint64_t h)
    {
        int i = (int)(s & (kInputRing - 1));
        step[i] = s; hash[i] = h; valid[i] = true;
    }
    bool Get(uint32_t s, uint64_t* h) const
    {
        int i = (int)(s & (kInputRing - 1));
        if (!valid[i] || step[i] != s)
            return false;
        *h = hash[i];
        return true;
    }
};

struct Session
{
    UdpSocket      sock;
    NetRole        role;
    NetState       state;
    NetFailReason  fail;
    NetSessionConfig cfg;

    NetAddress     peer;
    bool           peerKnown;

    InputRing      local;
    InputRing      remote;
    DigestRing     localDigest;

    uint32_t       localHighest;    // highest step we have scheduled input for
    uint32_t       remoteHighest;   // highest step we have the peer's input for

    double         lastRecvTime;
    double         lastHelloTime;
    double         lastPingTime;
    double         lastStallSendTime;   // see kStallResendInterval
    double         connectStart;
    uint64_t       pingToken;
    bool           pingOutstanding;
    double         rtt;

    bool           desynced;
    uint32_t       desyncStep;

    uint32_t       salt;            // joiner's nonce, echoed in WELCOME so a
                                    // stale reply from a previous attempt is
                                    // not mistaken for this one's

    Session() { Reset(); }

    void Reset()
    {
        sock.Close();
        role  = NetRole_None;
        state = NetState_Idle;
        fail  = NetFail_None;
        cfg   = NetSessionConfig();
        peer  = NetAddress();
        peerKnown = false;
        local.Reset();
        remote.Reset();
        localDigest.Reset();
        localHighest  = 0;
        remoteHighest = 0;
        lastRecvTime  = 0.0;
        lastHelloTime = 0.0;
        lastPingTime  = 0.0;
        lastStallSendTime = 0.0;
        connectStart  = 0.0;
        pingToken     = 0;
        pingOutstanding = false;
        rtt = 0.0;
        desynced   = false;
        desyncStep = 0;
        salt = 0;
    }
};

Session gS;

void WriteHeader(Writer& w, uint8_t type)
{
    w.U32(kMagic);
    w.U8(type);
    w.U8(0);
    w.U16(kNetProtocolVersion);
}

void SendTo(const NetAddress& to, const uint8_t* buf, int n)
{
    if (n <= 0 || !gS.sock.IsOpen())
        return;
    // A -1 here is routine for UDP (a stale ICMP unreachable surfaces on the
    // next call on some stacks) and is never grounds for ending the session.
    // The session timeout is the only thing that decides a peer is gone.
    gS.sock.Send(to, buf, n);
}

void SendSimple(const NetAddress& to, uint8_t type)
{
    uint8_t buf[kMaxPacket];
    Writer w(buf, sizeof(buf));
    WriteHeader(w, type);
    if (w.Ok())
        SendTo(to, buf, w.Size());
}

void SendReject(const NetAddress& to, NetFailReason reason)
{
    uint8_t buf[kMaxPacket];
    Writer w(buf, sizeof(buf));
    WriteHeader(w, Pkt_Reject);
    w.U8((uint8_t)reason);
    if (w.Ok())
        SendTo(to, buf, w.Size());
}

void SendHello(double now)
{
    uint8_t buf[kMaxPacket];
    Writer w(buf, sizeof(buf));
    WriteHeader(w, Pkt_Hello);
    w.U16(kNetSimVersion);
    w.U32(gS.salt);
    if (w.Ok())
        SendTo(gS.peer, buf, w.Size());
    gS.lastHelloTime = now;
}

void SendWelcome(const NetAddress& to, uint32_t salt)
{
    uint8_t buf[kMaxPacket];
    Writer w(buf, sizeof(buf));
    WriteHeader(w, Pkt_Welcome);
    w.U16(kNetSimVersion);
    w.U32(salt);
    w.U64(BitsOfDouble(gS.cfg.dt));
    w.U32(gS.cfg.seed);
    w.U16(gS.cfg.track);
    w.U16(gS.cfg.opponent);
    w.U8(gS.cfg.superLeague);
    if (w.Ok())
        SendTo(to, buf, w.Size());
}

// Send our recent inputs. Every packet repeats the last kRedundancy steps, so
// losing a run of packets shorter than that costs nothing — the data is
// already in the neighbours. Optionally carries a digest for the desync check.
void SendInputs()
{
    if (!gS.peerKnown)
        return;

    uint8_t buf[kMaxPacket];
    Writer w(buf, sizeof(buf));
    WriteHeader(w, Pkt_Input);

    const uint32_t base = gS.localHighest;

    // Don't run off the bottom at the start of a race.
    int count = kRedundancy;
    if ((uint32_t)count > base + 1)
        count = (int)(base + 1);

    w.U32(base);
    w.U8((uint8_t)count);
    for (int k = count - 1; k >= 0; --k)
    {
        uint32_t step = base - (uint32_t)k;
        w.U32(gS.local.Has(step) ? gS.local.Get(step) : 0u);
    }

    // Attach the most recent digest-interval digest we hold, if any.
    uint32_t dStep = (base / kDigestInterval) * kDigestInterval;
    uint64_t dHash = 0;
    if (gS.localDigest.Get(dStep, &dHash))
    {
        w.U8(1);
        w.U32(dStep);
        w.U64(dHash);
    }
    else
    {
        w.U8(0);
    }

    if (w.Ok())
        SendTo(gS.peer, buf, w.Size());
}

// Both peers prefill steps 0 .. kInputDelay-1 with neutral input. Nothing ever
// submits those steps — input sampled at step N is scheduled for N+kInputDelay,
// so the first kInputDelay steps have no author. Both sides fill them
// identically, so the simulation stays deterministic and the race can start
// without waiting a round trip.
void PrimeDelayWindow()
{
    for (uint32_t s = 0; s < (uint32_t)kInputDelay; ++s)
    {
        gS.local.Set(s, 0);
        gS.remote.Set(s, 0);
    }
    gS.localHighest  = (uint32_t)kInputDelay - 1;
    gS.remoteHighest = (uint32_t)kInputDelay - 1;
}

void EnterConnected(double now)
{
    gS.state        = NetState_Connected;
    gS.lastRecvTime = now;
    PrimeDelayWindow();

    char s[128];
    NetAddressToString(gS.peer, s, sizeof(s));
    printf("net: connected to %s as %s — track %u, seed %08x, dt %.9f (%.2fHz), "
           "input delay %d steps\n",
           s, gS.role == NetRole_Host ? "host" : "joiner",
           (unsigned)gS.cfg.track, gS.cfg.seed, gS.cfg.dt,
           gS.cfg.dt > 0.0 ? 1.0 / gS.cfg.dt : 0.0, kInputDelay);
    fflush(stdout);
}

void Failed(NetFailReason r)
{
    gS.state = NetState_Failed;
    gS.fail  = r;
    printf("net: session failed — %s\n", NetFailReasonText(r));
    fflush(stdout);
}

void HandleHello(Reader& r, const NetAddress& from, double now)
{
    if (gS.role != NetRole_Host)
        return;

    uint16_t simVer = r.U16();
    uint32_t salt   = r.U32();
    if (r.Bad())
        return;

    // Already playing someone else: refuse rather than let a third machine
    // stomp the session. Two players only.
    if (gS.state == NetState_Connected && gS.peerKnown && !(from == gS.peer))
    {
        SendReject(from, NetFail_PeerClosed);
        return;
    }

    if (simVer != kNetSimVersion)
    {
        printf("net: refusing joiner — sim version %u, we are %u. The two "
               "builds would desync.\n", (unsigned)simVer, (unsigned)kNetSimVersion);
        fflush(stdout);
        SendReject(from, NetFail_SimVersion);
        return;
    }

    // Accept. HELLO is resent until answered, so this legitimately arrives
    // again after we are already connected — answer it again and stay put,
    // rather than restarting the session under the peer.
    gS.peer      = from;
    gS.peerKnown = true;
    SendWelcome(from, salt);

    if (gS.state != NetState_Connected)
        EnterConnected(now);
    else
        gS.lastRecvTime = now;
}

void HandleWelcome(Reader& r, const NetAddress& from, double now)
{
    if (gS.role != NetRole_Joiner)
        return;
    if (gS.state == NetState_Connected)
    {
        gS.lastRecvTime = now;
        return;                     // a duplicate answer to a resent HELLO
    }

    uint16_t simVer  = r.U16();
    uint32_t salt    = r.U32();
    uint64_t dtBits  = r.U64();
    uint32_t seed    = r.U32();
    uint16_t track   = r.U16();
    uint16_t opp     = r.U16();
    uint8_t  league  = r.U8();
    if (r.Bad())
        return;

    // Ignore a WELCOME answering some earlier attempt.
    if (salt != gS.salt)
        return;

    if (simVer != kNetSimVersion)
    {
        Failed(NetFail_SimVersion);
        return;
    }

    double dt = DoubleOfBits(dtBits);
    if (!(dt > 0.0) || !(dt < 1.0))
    {
        Failed(NetFail_DtMismatch);
        return;
    }

    // Adopt the host's configuration wholesale. Agreeing is the whole point;
    // negotiating would only create ways to disagree.
    gS.cfg.dt       = dt;
    gS.cfg.seed     = seed;
    gS.cfg.track       = track;
    gS.cfg.opponent    = opp;
    gS.cfg.superLeague = (league != 0) ? 1 : 0;

    gS.peer      = from;
    gS.peerKnown = true;
    EnterConnected(now);
}

// Record a divergence and, if we found it ourselves, tell the peer.
//
// Detection is symmetric in principle -- each side compares the digest it
// receives against its own -- but in practice whoever notices first stops
// sending inputs, and the other side then just sees silence. Without this
// notification that silence surfaces as "the other machine did not answer",
// which sends the player looking for a network fault that isn't there. Say
// what actually happened.
void MarkDesync(uint32_t step, uint64_t ours, uint64_t theirs, bool notifyPeer)
{
    if (gS.desynced)
        return;

    gS.desynced   = true;
    gS.desyncStep = step;
    gS.state      = NetState_Desynced;
    gS.fail       = NetFail_Desync;

    if (notifyPeer && gS.peerKnown)
    {
        uint8_t buf[kMaxPacket];
        Writer w(buf, sizeof(buf));
        WriteHeader(w, Pkt_Desync);
        w.U32(step);
        w.U64(ours);
        if (w.Ok())
        {
            // Sent more than once: this is the last thing we will say, and if
            // it is lost the peer is back to guessing from silence.
            SendTo(gS.peer, buf, w.Size());
            SendTo(gS.peer, buf, w.Size());
            SendTo(gS.peer, buf, w.Size());
        }
    }

    printf("net: DESYNC at step %u — ours %016llx, peer %016llx.\n"
           "     The simulations have parted company; nothing after this point "
           "is shared.\n",
           (unsigned)step,
           (unsigned long long)ours, (unsigned long long)theirs);
    fflush(stdout);
}

void HandleInput(Reader& r, double now)
{
    if (gS.state != NetState_Connected && gS.state != NetState_Stalled)
        return;

    uint32_t base  = r.U32();
    uint8_t  count = r.U8();
    if (r.Bad() || count == 0 || count > kRedundancy)
        return;
    if ((uint32_t)count > base + 1)
        return;                      // claims steps below zero

    // Read into a scratch first: a truncated packet must not leave half its
    // steps applied, since a wrong input is a desync rather than a hiccup.
    uint32_t vals[kRedundancy];
    for (int k = 0; k < count; ++k)
        vals[k] = r.U32();

    uint8_t  hasDigest = r.U8();
    uint32_t dStep = 0;
    uint64_t dHash = 0;
    if (hasDigest)
    {
        dStep = r.U32();
        dHash = r.U64();
    }
    if (r.Bad())
        return;

    for (int k = 0; k < count; ++k)
    {
        uint32_t step = base - (uint32_t)(count - 1 - k);
        // Never overwrite: the first copy of a step is authoritative, and
        // redundant copies are by definition identical. Rewriting a step the
        // sim has already consumed would be a silent divergence.
        if (!gS.remote.Has(step))
            gS.remote.Set(step, vals[k]);
    }
    if (base > gS.remoteHighest)
        gS.remoteHighest = base;

    if (hasDigest && !gS.desynced)
    {
        uint64_t ours = 0;
        if (gS.localDigest.Get(dStep, &ours) && ours != dHash)
            MarkDesync(dStep, ours, dHash, true);
    }

    gS.lastRecvTime = now;
}

void HandlePacket(const uint8_t* buf, int n, const NetAddress& from, double now)
{
    if (n < kHeaderBytes)
        return;

    Reader r(buf, n);
    if (r.U32() != kMagic)
        return;                     // not ours; some other traffic on the port

    uint8_t  type = r.U8();
    r.U8();                         // flags, unused
    uint16_t ver  = r.U16();
    if (r.Bad())
        return;

    if (ver != kNetProtocolVersion)
    {
        // Only answer a HELLO — replying to anything else risks a loop with a
        // peer that cannot parse the reply either.
        if (type == Pkt_Hello && gS.role == NetRole_Host)
            SendReject(from, NetFail_ProtocolVersion);
        else if (gS.role == NetRole_Joiner && gS.state == NetState_Connecting)
            Failed(NetFail_ProtocolVersion);
        return;
    }

    // Once connected, ignore anyone who is not the peer. Two players only, and
    // it keeps a stray packet from injecting inputs.
    if ((gS.state == NetState_Connected || gS.state == NetState_Stalled)
        && gS.peerKnown && !(from == gS.peer) && type != Pkt_Hello)
        return;

    switch (type)
    {
    case Pkt_Hello:
        HandleHello(r, from, now);
        break;

    case Pkt_Welcome:
        HandleWelcome(r, from, now);
        break;

    case Pkt_Reject:
    {
        uint8_t reason = r.U8();
        if (r.Bad() || gS.role != NetRole_Joiner)
            break;
        if (gS.state == NetState_Connecting)
            Failed(reason <= NetFail_Desync ? (NetFailReason)reason : NetFail_Timeout);
        break;
    }

    case Pkt_Input:
        HandleInput(r, now);
        break;

    case Pkt_Desync:
    {
        uint32_t step  = r.U32();
        uint64_t their = r.U64();
        if (r.Bad())
            break;
        // The peer found it. Don't notify back -- it already knows, and a
        // mutual announcement would just cross on the wire.
        uint64_t ours = 0;
        gS.localDigest.Get(step, &ours);
        MarkDesync(step, ours, their, false);
        gS.lastRecvTime = now;
        break;
    }

    case Pkt_Bye:
        if (gS.peerKnown && from == gS.peer)
        {
            gS.state = NetState_Closed;
            gS.fail  = NetFail_PeerClosed;
            printf("net: peer disconnected.\n");
            fflush(stdout);
        }
        break;

    case Pkt_Ping:
    {
        uint64_t token = r.U64();
        if (r.Bad())
            break;
        uint8_t buf2[kMaxPacket];
        Writer w(buf2, sizeof(buf2));
        WriteHeader(w, Pkt_Pong);
        w.U64(token);
        if (w.Ok())
            SendTo(from, buf2, w.Size());
        gS.lastRecvTime = now;
        break;
    }

    case Pkt_Pong:
    {
        uint64_t token = r.U64();
        if (r.Bad())
            break;
        if (gS.pingOutstanding && token == gS.pingToken)
        {
            double sample = now - DoubleOfBits(gS.pingToken);
            if (sample >= 0.0 && sample < 10.0)
            {
                // Exponential smoothing. A single sample is mostly scheduling
                // noise; the UI wants the trend.
                gS.rtt = (gS.rtt > 0.0) ? (gS.rtt * 0.75 + sample * 0.25) : sample;
            }
            gS.pingOutstanding = false;
        }
        gS.lastRecvTime = now;
        break;
    }

    default:
        break;
    }
}

} // namespace

// --- Public API ------------------------------------------------------------

const char* NetFailReasonText(NetFailReason r)
{
    switch (r)
    {
    case NetFail_None:            return "no error";
    case NetFail_SocketError:     return "could not open the socket";
    case NetFail_Timeout:         return "the other machine did not answer";
    case NetFail_ProtocolVersion: return "the two builds speak different network protocols";
    case NetFail_SimVersion:      return "the two builds have different physics and would desync";
    case NetFail_DtMismatch:      return "the two machines could not agree a timestep";
    case NetFail_PeerClosed:      return "the other player disconnected";
    case NetFail_Desync:          return "the simulations diverged";
    }
    return "unknown";
}

bool NetHost(uint16_t port, const NetSessionConfig& cfg, double now)
{
    NetClose();
    gS.Reset();

    if (!NetInit())
    {
        Failed(NetFail_SocketError);
        return false;
    }
    if (!gS.sock.Open(port))
    {
        Failed(NetFail_SocketError);
        NetShutdown();          // balance the NetInit above; NetClose won't,
                                // because the role was never set
        return false;
    }

    gS.role         = NetRole_Host;
    gS.state        = NetState_Listening;
    gS.cfg          = cfg;
    gS.connectStart = now;
    gS.lastRecvTime = now;

    printf("net: hosting on port %u — waiting for the other player.\n",
           (unsigned)gS.sock.LocalPort());
    fflush(stdout);
    return true;
}

bool NetJoin(const char* host, uint16_t port, double now)
{
    NetClose();
    gS.Reset();

    if (!NetInit())
    {
        Failed(NetFail_SocketError);
        return false;
    }
    // Ephemeral local port: only the host needs a predictable one.
    if (!gS.sock.Open(0))
    {
        Failed(NetFail_SocketError);
        NetShutdown();
        return false;
    }
    if (!NetResolve(host, port, &gS.peer))
    {
        Failed(NetFail_SocketError);
        NetShutdown();
        return false;
    }

    gS.role         = NetRole_Joiner;
    gS.state        = NetState_Connecting;
    gS.peerKnown    = true;
    gS.connectStart = now;
    gS.lastRecvTime = now;

    // Distinguishes this attempt from an earlier one, so a late WELCOME for a
    // previous connect cannot be mistaken for an answer to this one.
    gS.salt = (uint32_t)(BitsOfDouble(now) ^ (BitsOfDouble(now) >> 32)) | 1u;

    char s[128];
    NetAddressToString(gS.peer, s, sizeof(s));
    printf("net: connecting to %s...\n", s);
    fflush(stdout);

    SendHello(now);
    return true;
}

void NetClose()
{
    if (gS.state == NetState_Connected || gS.state == NetState_Stalled
        || gS.state == NetState_Desynced)
    {
        if (gS.peerKnown)
            SendSimple(gS.peer, Pkt_Bye);
    }
    bool had = (gS.role != NetRole_None);
    gS.Reset();
    if (had)
        NetShutdown();
}

void NetPoll(double now)
{
    if (gS.state == NetState_Idle || !gS.sock.IsOpen())
        return;

    // Drain everything queued. Bounded so a flood cannot hold the frame loop.
    uint8_t buf[kMaxPacket * 2];
    for (int i = 0; i < 64; ++i)
    {
        NetAddress from;
        int n = gS.sock.Recv(buf, sizeof(buf), &from);
        if (n <= 0)
            break;
        HandlePacket(buf, n, from, now);
    }

    switch (gS.state)
    {
    case NetState_Connecting:
        if (now - gS.connectStart > kConnectTimeout)
            Failed(NetFail_Timeout);
        else if (now - gS.lastHelloTime > kHelloInterval)
            SendHello(now);
        break;

    case NetState_Listening:
        // Waits indefinitely — the player decides when to give up.
        break;

    case NetState_Connected:
    case NetState_Stalled:
        if (now - gS.lastRecvTime > kSessionTimeout)
        {
            Failed(NetFail_Timeout);
            break;
        }
        // Break a stall deadlock: see kStallResendInterval. Only while actually
        // stalled, so a healthy race still sends exactly one packet per step.
        if (gS.state == NetState_Stalled &&
            now - gS.lastStallSendTime > kStallResendInterval)
        {
            gS.lastStallSendTime = now;
            SendInputs();
        }

        if (!gS.pingOutstanding && now - gS.lastPingTime > kPingInterval)
        {
            gS.pingToken       = BitsOfDouble(now);
            gS.pingOutstanding = true;
            gS.lastPingTime    = now;

            uint8_t p[kMaxPacket];
            Writer w(p, sizeof(p));
            WriteHeader(w, Pkt_Ping);
            w.U64(gS.pingToken);
            if (w.Ok())
                SendTo(gS.peer, p, w.Size());
        }
        else if (gS.pingOutstanding && now - gS.lastPingTime > kPingInterval * 4.0)
        {
            gS.pingOutstanding = false;     // lost; try again next interval
        }
        break;

    default:
        break;
    }
}

NetState      NetGetState()      { return gS.state; }
NetRole       NetGetRole()       { return gS.role; }
NetFailReason NetGetFailReason() { return gS.fail; }
double        NetGetRTT()        { return gS.rtt; }
uint16_t      NetLocalPort()     { return gS.sock.LocalPort(); }
bool          NetHasDesync()     { return gS.desynced; }
uint32_t      NetDesyncStep()    { return gS.desyncStep; }

const NetSessionConfig& NetGetConfig() { return gS.cfg; }

void NetSubmitInput(uint32_t step, uint32_t input)
{
    if (gS.state != NetState_Connected && gS.state != NetState_Stalled)
        return;

    const uint32_t target = step + (uint32_t)kInputDelay;
    if (gS.local.Has(target))
        return;                     // already scheduled; don't rewrite history

    gS.local.Set(target, input);
    if (target > gS.localHighest)
        gS.localHighest = target;

    SendInputs();
}

bool NetStepReady(uint32_t step)
{
    if (gS.state == NetState_Desynced)
        return false;
    if (gS.state != NetState_Connected && gS.state != NetState_Stalled)
        return false;

    bool ready = gS.local.Has(step) && gS.remote.Has(step);

    // Surface the stall so the caller can say why the picture has frozen,
    // rather than the game just appearing to hang.
    if (!ready && gS.state == NetState_Connected)
        gS.state = NetState_Stalled;
    else if (ready && gS.state == NetState_Stalled)
        gS.state = NetState_Connected;

    return ready;
}

uint32_t NetLocalInput(uint32_t step)
{
    return gS.local.Has(step) ? gS.local.Get(step) : 0u;
}

uint32_t NetRemoteInput(uint32_t step)
{
    return gS.remote.Has(step) ? gS.remote.Get(step) : 0u;
}

void NetReportDigest(uint32_t step, uint64_t digest)
{
    if (step % (uint32_t)kDigestInterval != 0)
        return;
    gS.localDigest.Set(step, digest);
}

} // namespace scr
