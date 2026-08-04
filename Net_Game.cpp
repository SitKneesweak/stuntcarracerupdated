// Net_Game — see Net_Game.h.

#include "Net_Game.h"

#include <cstdio>
#include <cstring>

#include "Det_Rand.h"
#include "Net_Lockstep.h"
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
    uint32_t step;

    char     status[160];

    GameSession() { Reset(); }

    void Reset()
    {
        active    = false;
        isHost    = false;
        announced = false;
        racing    = false;
        track     = -1;
        step      = 0;
        status[0] = '\0';
    }
};

GameSession gG;

// Restore whatever the session forced, so quitting a race leaves the game in
// the state a single-player session expects.
void UnlockSettings()
{
    gSimSettingsLocked = false;
}

} // namespace

bool NetGameHost(int trackID, double now)
{
    NetGameCancel();

    NetSessionConfig cfg;
    cfg.dt    = gFloatV2Dt;
    cfg.track = (uint16_t)trackID;
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
    gG.active = true;
    gG.isHost = true;
    gG.track  = trackID;
    snprintf(gG.status, sizeof(gG.status),
             "Waiting for the other player on port %u...", (unsigned)NetLocalPort());
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
    gG.Reset();
}

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
            gG.track     = (int)cfg.track;
            gFloatV2Dt   = cfg.dt;
            gUseFloatV2Physics = true;

            snprintf(gG.status, sizeof(gG.status),
                     "Connected as %s - track %d.",
                     gG.isHost ? "host" : "joiner", gG.track);
            return true;
        }
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

const char* NetGameStatusLine() { return gG.status; }

bool NetGameFailed()
{
    if (!gG.active)
        return false;

    const NetState st = NetGetState();
    return (st == NetState_Failed) || (st == NetState_Desynced) || (st == NetState_Closed);
}

void NetGameRaceBegun()
{
    if (!gG.active)
        return;

    gG.step   = 0;
    gG.racing = true;

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

} // namespace scr
