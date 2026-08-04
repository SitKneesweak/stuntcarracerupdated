// Net_Lockstep — two-player deterministic lockstep over UDP.
//
// The model, and why it is this one: both peers run the identical simulation
// and exchange only *inputs* (the 5-bit KEY_P1_* mask from Car_Behaviour.h).
// Neither peer's car is authoritative, because both compute both cars. That is
// what the determinism pass in Det_Math.h / Det_Rand.h / Sim_Trace.cpp was for
// — without bit-identical physics this design does not work at all.
//
// Note the Amiga did NOT do this. Its serial link-up (StuntCarRacer.s:3309)
// sent car *state* — road section, distance, suspension heights, z speed —
// every frame, and each machine simulated only its own car. That needs no
// determinism and tolerates loss natively, but the two machines disagree
// slightly about collisions. Lockstep is a deliberate improvement, not a
// reproduction. If it ever proves unworkable, that is the fallback.
//
// --- Input delay -----------------------------------------------------------
// Pure lockstep cannot simulate step N until the peer's input for step N has
// arrived, so a naive implementation stalls for a full round trip every step.
// Instead, input sampled during step N is scheduled for step N + kInputDelay.
// Both peers do this, so the peer's input for step N was sent kInputDelay steps
// ago and has had that long to travel. The cost is that the controls feel
// kInputDelay steps late; at 60Hz the default 3 is 50ms, and it covers an RTT
// of about that before the game starts stalling.
//
// --- Loss ------------------------------------------------------------------
// UDP, deliberately. TCP's head-of-line blocking is actively harmful here: a
// retransmit stalls every later step behind the lost one, which is the exact
// hitch lockstep is trying to avoid. Instead every packet repeats the last
// kRedundancy inputs, so losing up to kRedundancy-1 consecutive packets costs
// nothing at all — the input was already in the packets either side of it.
// Inputs are 4 bytes, so this is very cheap.
//
// --- Desync ----------------------------------------------------------------
// Track 7 (RollerCoaster) is known to diverge between macOS and Windows and is
// unresolved (see the determinism notes). Rather than block on it, peers trade
// the Sim_Trace state digest every kDigestInterval steps. A mismatch means the
// simulations have parted company; the session reports Desynced rather than
// letting the two players drift into separate realities without being told.
//
// This header includes only <cstdint> and Net_Socket.h, and Net_Socket.h
// includes only <cstdint>. Nothing here may include a platform header —
// see the note at the top of Net_Socket.h.

#ifndef NET_LOCKSTEP_H
#define NET_LOCKSTEP_H

#include <cstdint>

#include "Net_Socket.h"

namespace scr {

// Bumped whenever the wire format changes. Peers with different values refuse
// to connect rather than misparse each other.
const uint16_t kNetProtocolVersion = 1;

// Bumped whenever anything reachable from PhysicsStepF_Tick changes in a way
// that could alter results. Two peers on different sim versions would desync
// immediately and mysteriously; this turns that into a clear refusal at
// connect. THIS IS MANUAL — if you change the physics, change this.
// 2: the two-car head-to-head wiring. The physics itself is byte-for-byte what
//    version 1 computed (the simtrace digests are unchanged), but what a *race*
//    depends on is not: both cars are now stepped in role order and the start
//    placement moved off the crane's re-lift offset onto a small one. A version
//    1 peer would place its cars elsewhere and diverge immediately.
const uint16_t kNetSimVersion = 2;

// Steps of input delay. See the header comment.
const int kInputDelay = 3;

// How many past steps of input each packet repeats.
const int kRedundancy = 8;

// Exchange a state digest every this many steps.
const int kDigestInterval = 30;

// Input history ring. Must comfortably exceed kInputDelay + kRedundancy plus
// whatever jitter buys us; a power of two so the modulo is a mask.
const int kInputRing = 256;

enum NetRole
{
    NetRole_None = 0,
    NetRole_Host,       // binds a known port and waits. Chooses track and seed.
    NetRole_Joiner      // connects out to the host's address.
};

enum NetState
{
    NetState_Idle = 0,      // no session
    NetState_Listening,     // host, waiting for a joiner
    NetState_Connecting,    // joiner, HELLO sent, waiting for WELCOME
    NetState_Connected,     // handshake done, exchanging inputs
    NetState_Stalled,       // connected but waiting on the peer's input
    NetState_Desynced,      // digest mismatch — the sims have parted company
    NetState_Failed,        // handshake refused or timed out
    NetState_Closed         // peer said goodbye, or we did
};

// What both peers must agree on before a race can start. The host decides all
// of it and the joiner adopts it wholesale — agreeing is what matters, and a
// negotiation would only add ways to disagree.
struct NetSessionConfig
{
    double   dt;        // gFloatV2Dt. Exchanged as its bit pattern, not a
                        // rounded decimal, since it must match exactly.
    uint32_t seed;      // Det_Rand seed for the shared RNG stream
    uint16_t track;     // which circuit
    uint16_t opponent;  // which opponent car/driver slot the remote player uses

    NetSessionConfig() : dt(1.0 / 60.0), seed(0), track(0), opponent(0) {}
};

// Why a session ended or was refused, for the UI. Kept as an enum rather than a
// string so the caller can decide the wording.
enum NetFailReason
{
    NetFail_None = 0,
    NetFail_SocketError,
    NetFail_Timeout,            // no response within the connect timeout
    NetFail_ProtocolVersion,    // peer speaks a different wire format
    NetFail_SimVersion,         // peer's physics differs — would desync
    NetFail_DtMismatch,         // peer could not adopt our timestep
    NetFail_PeerClosed,
    NetFail_Desync
};

// --- Session lifetime ------------------------------------------------------
// All times are seconds from any monotonic source, passed in rather than read
// from a platform clock, which keeps this module free of platform headers and
// lets the tests drive it deterministically. The game passes DXUTGetTime().

// Bind and wait for a joiner. cfg is this side's choice of track/seed/dt.
bool NetHost(uint16_t port, const NetSessionConfig& cfg, double now);

// Connect out. host may be a name or a v4/v6 literal. Blocking DNS — call from
// the menu, never the frame loop.
bool NetJoin(const char* host, uint16_t port, double now);

// Pump the socket. Call once per rendered frame, before stepping the sim.
void NetPoll(double now);

// Say goodbye and drop the session.
void NetClose();

NetState      NetGetState();
NetRole       NetGetRole();
NetFailReason NetGetFailReason();
const char*   NetFailReasonText(NetFailReason r);

// Valid once the state reaches Connected — for the joiner this is the host's
// configuration, not the one it asked for.
const NetSessionConfig& NetGetConfig();

// The port actually bound, for the host UI to display after NetHost(0, ...).
uint16_t NetLocalPort();

// Round-trip time in seconds, smoothed. 0 until measured.
double NetGetRTT();

// --- Per-step input exchange -----------------------------------------------
// The step counter is the shared clock. Both peers start at step 0 on the first
// simulated step of the race and increment in lockstep; it is not wall time and
// must never be derived from it.

// Submit the local player's freshly sampled input while simulating `step`. It
// is scheduled for step + kInputDelay and sent immediately, along with the
// preceding kRedundancy-1 inputs. Call once per step, before NetStepReady.
void NetSubmitInput(uint32_t step, uint32_t input);

// True when both peers' inputs for `step` are known and it can be simulated.
// False means the peer's input has not arrived — the caller must NOT simulate;
// it should render the previous frame again and try next frame. This is the
// stall that input delay exists to make rare.
bool NetStepReady(uint32_t step);

// Inputs for a step that NetStepReady has approved. Reading a step that is not
// ready returns the last known input for that peer, which is a guess — check
// NetStepReady first.
uint32_t NetLocalInput(uint32_t step);
uint32_t NetRemoteInput(uint32_t step);

// Feed the Sim_Trace digest for a completed step. Cheap and ignored except on
// digest-interval steps, so the caller can hand over every step without
// worrying about it.
void NetReportDigest(uint32_t step, uint64_t digest);

// Set once a digest mismatch has been seen. The session state also becomes
// NetState_Desynced; this is the detail for the message.
bool     NetHasDesync();
uint32_t NetDesyncStep();

} // namespace scr

#endif // NET_LOCKSTEP_H
