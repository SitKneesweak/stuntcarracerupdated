// Net_Game — see Net_Game.h.

#include "Net_Game.h"

#include <cstdio>
#include <cstring>

#include "Det_Rand.h"
#include "Net_Lockstep.h"
#include "Net_Socket.h"
#include "Physics_FloatV2.h"

namespace scr {

namespace {

struct GameSession
{
    bool     active;        // NetGameHost/NetGameJoin succeeded and we have not cancelled
    bool     isHost;
    bool     announced;     // the "just connected" edge has been reported once
    bool     racing;
    int      track;         // -1 until the joiner is told
    bool     superLeague;   // likewise adopted from the host
    uint32_t step;

    char     status[160];

    // --- The season ---
    NetLeagueDriver driver[2];      // indexed by NetCarOwner
    int      racesRun;
    bool     trackUsed[kNetSeasonRaces];
    int      nextTrack;             // -1 until the host has chosen and it has landed
    bool     peerForfeited;         // edge, cleared by NetGamePeerForfeited
    char     localName[16];
    bool     nameSent;

    GameSession() { Reset(); }

    void Reset()
    {
        active    = false;
        isHost    = false;
        announced = false;
        racing      = false;
        track       = -1;
        superLeague = false;
        step        = 0;
        status[0] = '\0';

        /*  The table lives exactly as long as the session, so it is cleared here
            with everything else. Reset() runs on cancel and on opening a new
            session, never between races.                                       */
        ClearLeague();
        localName[0] = '\0';
        nameSent     = false;
    }

    void ClearLeague()
    {
        for (int i = 0; i < 2; ++i)
        {
            NetLeagueDriver& d = driver[i];
            d.name[0] = '\0';
            d.raced = d.won = d.lost = d.wrecked = d.bestLaps = d.points = 0;
        }
        racesRun = 0;
        /*  Indexed by track, not by race number - they are the same count only
            because the season is one race per circuit.                          */
        for (int i = 0; i < kNetSeasonRaces; ++i)
            trackUsed[i] = false;
        nextTrack     = -1;
        peerForfeited = false;
    }
};

GameSession gG;

// Restore whatever the session forced, so quitting a race leaves the game in
// the state a single-player session expects.
void UnlockSettings()
{
    gSimSettingsLocked = false;
}

// --- What crosses the control channel --------------------------------------
// Three messages, all of them menu traffic. Every one carries the race number
// it belongs to except the name, which belongs to the session: the channel is
// reliable but not instantaneous, and a next-track choice that arrives after
// the recipient has already moved on must be recognised as stale rather than
// applied to the wrong fixture.

enum CtrlTag
{
    Ctrl_Name      = 1,     // name[16]
    Ctrl_NextTrack = 2,     // raceIndex, track
    Ctrl_Forfeit   = 3      // raceIndex
};

// One in flight at a time on the wire, so anything raised while the channel is
// busy waits here. Four is far more than the menus can generate - a name, a
// track choice and a forfeit is the realistic worst case.
struct Outbox
{
    uint8_t msg[4][kMaxControlBytes];
    int     len[4];
    int     count;

    void Clear() { count = 0; }

    void Push(const uint8_t* p, int n)
    {
        if ((count >= 4) || (n <= 0) || (n > kMaxControlBytes))
            return;
        for (int i = 0; i < n; ++i)
            msg[count][i] = p[i];
        len[count] = n;
        ++count;
    }

    void Pop()
    {
        if (count <= 0)
            return;
        for (int i = 1; i < count; ++i)
        {
            for (int b = 0; b < len[i]; ++b)
                msg[i - 1][b] = msg[i][b];
            len[i - 1] = len[i];
        }
        --count;
    }
};

Outbox gOut;

void QueueControl(const uint8_t* p, int n, double now)
{
    gOut.Push(p, n);

    // Send straight away if the wire is free; otherwise the pump in
    // NetGamePoll picks it up.
    if (NetControlIdle() && (gOut.count > 0) && NetSendControl(gOut.msg[0], gOut.len[0], now))
        gOut.Pop();
}

} // namespace

bool NetGameHost(int trackID, bool superLeague, double now)
{
    NetGameCancel();

    NetSessionConfig cfg;
    cfg.dt          = gFloatV2Dt;
    cfg.track       = (uint16_t)trackID;
    cfg.superLeague = superLeague ? 1 : 0;
    // The seed has to be agreed, not negotiated, and the host is the one who
    // decides. Derived from the clock so two sessions on the same track are not
    // identical races; the joiner is told the value and never rolls its own.
    cfg.seed  = (uint32_t)(now * 1000.0) ^ 0x5bf03635u;
    if (cfg.seed == 0)
        cfg.seed = 0x9E3779B9u;

    if (!NetHost(kNetDefaultPort, cfg, now))
    {
        snprintf(gG.status, sizeof(gG.status),
                 "Could not open port %u.", (unsigned)kNetDefaultPort);
        return false;
    }

    gG.Reset();
    gG.active      = true;
    gG.isHost      = true;
    gG.track       = trackID;
    gG.superLeague = superLeague;
    // The joining player has to type this machine's address, and nothing else
    // in the game ever tells them what it is, so the host screen has to. Over
    // the internet it is the router's public address that matters and UDP
    // kNetDefaultPort has to be forwarded - but on a LAN, which is how this
    // gets played, one of these is the answer.
    char local[96];
    NetLocalAddresses(local, sizeof(local));
    snprintf(gG.status, sizeof(gG.status),
             "Waiting on port %u. Tell the other player to join: %s",
             (unsigned)NetLocalPort(), local);
    return true;
}

bool NetGameJoin(const char* address, double now)
{
    NetGameCancel();

    if ((address == NULL) || (address[0] == '\0'))
    {
        snprintf(gG.status, sizeof(gG.status), "Enter an address first.");
        return false;
    }

    if (!NetJoin(address, kNetDefaultPort, now))
    {
        snprintf(gG.status, sizeof(gG.status), "Could not reach %s.", address);
        return false;
    }

    gG.Reset();
    gG.active = true;
    gG.isHost = false;
    gG.track  = -1;         // the host will tell us
    snprintf(gG.status, sizeof(gG.status), "Connecting to %s...", address);
    return true;
}

void NetGameCancel()
{
    if (gG.active)
        NetClose();
    UnlockSettings();
    gOut.Clear();
    gG.Reset();
}

namespace {

// Drain one queued control message onto the wire, and take delivery of
// whatever the peer has sent. Called from NetGamePoll, so this runs during a
// race as well as between races - a forfeit has to reach a peer who is still
// driving.
void PumpControl(double now)
{
    if (NetControlIdle() && (gOut.count > 0) &&
        NetSendControl(gOut.msg[0], gOut.len[0], now))
        gOut.Pop();

    uint8_t buf[kMaxControlBytes];
    const int n = NetReceiveControl(buf, sizeof(buf));
    if (n <= 0)
        return;

    switch (buf[0])
    {
    case Ctrl_Name:
    {
        if (n < 2)
            break;
        NetLeagueDriver& them = gG.driver[gG.isHost ? NetCar_Joiner : NetCar_Host];
        int len = n - 1;
        if (len > (int)sizeof(them.name) - 1)
            len = (int)sizeof(them.name) - 1;
        memcpy(them.name, buf + 1, (size_t)len);
        them.name[len] = '\0';
        break;
    }

    case Ctrl_NextTrack:
    {
        if (n < 3)
            break;
        // Stale: it names a fixture that has already been raced. Dropping it is
        // right - the host will have sent a fresh one for the current race.
        if ((int)buf[1] != gG.racesRun)
            break;
        const int track = (int)buf[2];
        if ((track >= 0) && (track < kNetSeasonRaces))
        {
            // Both, and for different readers: nextTrack is the menu's cue to
            // follow the host into the race, and track is what the race itself
            // is started on.
            gG.nextTrack = track;
            gG.track     = track;
        }
        break;
    }

    case Ctrl_Forfeit:
    {
        if (n < 2)
            break;
        if ((int)buf[1] != gG.racesRun)
            break;      // a forfeit for a race already scored

        // The peer quit, so this machine won it. Wrecks are not recorded
        // against an abandoned race: nobody's car was destroyed, it was left.
        const bool peerIsHost = !gG.isHost;
        NetGameLeagueScoreRace(!peerIsHost, false, false, -1);
        gG.peerForfeited = true;
        break;
    }

    default:
        break;      // a tag from a later version; ignoring it is correct
    }
}

} // namespace

bool NetGamePoll(double now)
{
    if (!gG.active)
        return false;

    NetPoll(now);

    const NetState st = NetGetState();

    switch (st)
    {
    case NetState_Connected:
    case NetState_Stalled:
        if (!gG.announced)
        {
            gG.announced = true;

            const NetSessionConfig& cfg = NetGetConfig();

            // The joiner adopts the host's settings wholesale. Agreeing is the
            // whole requirement; a negotiation would only add ways to disagree.
            gG.track       = (int)cfg.track;
            gG.superLeague = (cfg.superLeague != 0);
            gFloatV2Dt     = cfg.dt;
            gUseFloatV2Physics = true;

            // Our row of the table, and the peer's copy of it. The Amiga did
            // exactly this at link time (StuntCarRacer.s:4166) - names are the
            // first thing the two machines agree on.
            NetLeagueDriver& us = gG.driver[gG.isHost ? NetCar_Host : NetCar_Joiner];
            snprintf(us.name, sizeof(us.name), "%s",
                     (gG.localName[0] != '\0') ? gG.localName : "DRIVER");

            if (!gG.nameSent)
            {
                gG.nameSent = true;
                uint8_t m[1 + 16];
                m[0] = Ctrl_Name;
                const int len = (int)strlen(us.name);
                memcpy(m + 1, us.name, (size_t)len);
                QueueControl(m, 1 + len, now);
            }

            // The host's opening choice is already in the handshake; record it
            // so the table's first fixture and the season's used-track list
            // agree with what is about to be raced.
            gG.nextTrack = gG.track;

            snprintf(gG.status, sizeof(gG.status),
                     "Connected as %s - track %d, %s league.",
                     gG.isHost ? "host" : "joiner", gG.track,
                     gG.superLeague ? "super" : "standard");
            PumpControl(now);
            return true;
        }

        PumpControl(now);

        if (gG.racing && st == NetState_Stalled)
            snprintf(gG.status, sizeof(gG.status),
                     "Waiting for the other player...");
        else if (gG.racing)
            snprintf(gG.status, sizeof(gG.status),
                     "Connected - %.0fms.", NetGetRTT() * 1000.0);
        break;

    case NetState_Desynced:
        snprintf(gG.status, sizeof(gG.status),
                 "The two games have gone out of step at %u. The race cannot continue.",
                 (unsigned)NetDesyncStep());
        gG.racing = false;
        break;

    case NetState_Failed:
        snprintf(gG.status, sizeof(gG.status), "%s",
                 NetFailReasonText(NetGetFailReason()));
        gG.racing = false;
        break;

    case NetState_Closed:
        snprintf(gG.status, sizeof(gG.status), "The other player has left.");
        gG.racing = false;
        break;

    default:
        break;
    }

    return false;
}

bool NetGameSessionActive() { return gG.active; }

bool NetGameRacing()
{
    if (!gG.active || !gG.racing)
        return false;

    const NetState st = NetGetState();
    return (st == NetState_Connected) || (st == NetState_Stalled);
}

bool NetGameLocalIsHost() { return gG.isHost; }

int NetGameTrack() { return gG.track; }

bool NetGameSuperLeague() { return gG.superLeague; }

const char* NetGameStatusLine() { return gG.status; }

bool NetGameFailed()
{
    if (!gG.active)
        return false;

    const NetState st = NetGetState();
    return (st == NetState_Failed) || (st == NetState_Desynced) || (st == NetState_Closed);
}

bool NetGamePauseBlocked(const char* what)
{
    if (!NetGameRacing())
        return false;

    printf("%s is not available in a two-player race - it would freeze the other "
           "player's game too.\n", what ? what : "Pausing");
    fflush(stdout);
    return true;
}

void NetGameRaceBegun()
{
    if (!gG.active)
        return;

    /*  Each race of the season gets its own stretch of the step line rather than
        starting again at 0. The transport's input ring is keyed by step, so a
        second race beginning at 0 would find the first race's inputs already
        sitting at the steps it was about to ask about, and NetStepReady would
        wave through moves nobody had made.

        The base comes from the number of races run - which both peers score
        identically from their own copy of the simulation - and not from where
        the last race happened to stop. That matters: a race ended by a forfeit
        stops a few steps apart on the two machines, so anything derived from the
        previous race's final step would have the two ends starting this one on
        different numbers.                                                      */
    gG.step   = (uint32_t)gG.racesRun * kNetRaceStepBase;
    gG.racing = true;

    NetBeginRaceAt(gG.step);

    /*  Everything both peers had to agree on, on one line, at the moment it starts
        mattering. A mismatch in any of it is a desync a second or two later, and
        from inside the race they all look alike -- so print the inputs rather than
        leaving the reader to infer them from the divergence. */
    const NetSessionConfig& cfg = NetGetConfig();
    printf("net: race begins as %s -- track %u, %s league, dt %.9f, seed %08x\n",
           gG.isHost ? "host" : "joiner", (unsigned)cfg.track,
           cfg.superLeague ? "super" : "standard", cfg.dt, (unsigned)cfg.seed);
    fflush(stdout);

    // Both peers seed from the same value, so every SCR_Rand() draw in the sim
    // agrees. This is the whole reason Det_Rand exists.
    det::SeedRand(NetGetConfig().seed);

    // From here until the session ends, the debug keys that change what the sim
    // computes are refused: two peers must agree on all of it, and the timestep
    // key in particular desyncs a session the instant it is pressed.
    gSimSettingsLocked = true;
}

void NetGameRaceEnded()
{
    gG.racing = false;
    UnlockSettings();
}

uint32_t NetGameStep() { return gG.step; }

void NetGameAdvanceStep() { ++gG.step; }

// --- The two-driver league -------------------------------------------------

void NetGameSetLocalName(const char* name)
{
    snprintf(gG.localName, sizeof(gG.localName), "%s", (name != NULL) ? name : "");
}

void NetGameLeagueReset()
{
    gG.ClearLeague();
}

const NetLeagueDriver& NetGameLeagueDriver(NetCarOwner who)
{
    return gG.driver[(who == NetCar_Host) ? NetCar_Host : NetCar_Joiner];
}

void NetGameLeagueScoreRace(bool hostWon, bool hostWrecked, bool joinerWrecked, int bestLap)
{
    if (gG.racesRun >= kNetSeasonRaces)
        return;         // the season is over; nothing left to score

    NetLeagueDriver& host   = gG.driver[NetCar_Host];
    NetLeagueDriver& joiner = gG.driver[NetCar_Joiner];

    ++host.raced;
    ++joiner.raced;

    // Two for the win, as the Amiga's RESULT screen scores it.
    if (hostWon) { ++host.won;   ++joiner.lost; host.points   += 2; }
    else         { ++joiner.won; ++host.lost;   joiner.points += 2; }

    if (hostWrecked)   ++host.wrecked;
    if (joinerWrecked) ++joiner.wrecked;

    // And one for the fastest lap. -1 means neither driver completed one,
    // which a race decided by an early wreck can easily manage.
    if (bestLap == NetCar_Host)        { ++host.bestLaps;   ++host.points; }
    else if (bestLap == NetCar_Joiner) { ++joiner.bestLaps; ++joiner.points; }

    if ((gG.track >= 0) && (gG.track < kNetSeasonRaces))
        gG.trackUsed[gG.track] = true;

    ++gG.racesRun;

    // The fixture just raced is done with. The host chooses the next one from
    // the table screen; until it arrives there is nothing to start.
    gG.nextTrack = -1;
}

int  NetGameRacesRun()   { return gG.racesRun; }
bool NetGameSeasonOver() { return (gG.racesRun >= kNetSeasonRaces); }

bool NetGameTrackUsed(int track)
{
    if ((track < 0) || (track >= kNetSeasonRaces))
        return true;    // not a track anyone can pick
    return gG.trackUsed[track];
}

void NetGameProposeNextTrack(int track, double now)
{
    if (!gG.isHost || (track < 0) || (track >= kNetSeasonRaces))
        return;
    if (gG.trackUsed[track])
        return;         // one race per circuit

    gG.nextTrack = track;
    gG.track     = track;

    uint8_t m[3];
    m[0] = Ctrl_NextTrack;
    m[1] = (uint8_t)gG.racesRun;
    m[2] = (uint8_t)track;
    QueueControl(m, 3, now);
}

int NetGameNextTrack() { return gG.nextTrack; }

void NetGameForfeitRace(double now)
{
    if (!gG.active || !gG.racing)
        return;

    // Score it against ourselves first: the message may take a moment, and the
    // table this machine draws must already say what happened.
    const bool weAreHost = gG.isHost;
    NetGameLeagueScoreRace(!weAreHost, false, false, -1);

    uint8_t m[2];
    m[0] = Ctrl_Forfeit;
    // The race being conceded is the one just scored, so name it, not the next.
    m[1] = (uint8_t)(gG.racesRun - 1);
    QueueControl(m, 2, now);

    gG.racing = false;
    UnlockSettings();
}

bool NetGamePeerForfeited()
{
    const bool f = gG.peerForfeited;
    gG.peerForfeited = false;
    return f;
}

const char* NetGameRemoteName()
{
    return gG.driver[gG.isHost ? NetCar_Joiner : NetCar_Host].name;
}

} // namespace scr
