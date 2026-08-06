// Net_Game — the game's side of a lockstep session.
//
// Net_Lockstep.* is the transport: handshake, input exchange, desync detection,
// and nothing about Stunt Car Racer. This file is the thin layer between it and
// the game — session lifecycle as the menus see it, the step counter the race
// runs on, and the status text the waiting screen shows.
//
// It deliberately knows nothing about the car physics. Stepping the two cars
// happens in StuntCarRacer.cpp, where the player and opponent globals live;
// putting it here would mean including Car_Behaviour.h, which needs dx_linux.h,
// which must never meet a socket header (see the note in Net_Socket.h).
//
// Like Net_Lockstep.h and Net_Socket.h, this header includes only <cstdint>. It
// is safe to include from files that also include dx_linux.h.

#ifndef NET_GAME_H
#define NET_GAME_H

#include <cstdint>

namespace scr {

// Direct IP, so somebody has to port-forward this. Chosen from the unassigned
// range and otherwise arbitrary.
const uint16_t kNetDefaultPort = 27500;

// Which of the two cars a player drives. Derived from the network ROLE, never
// from "am I local" — both peers simulate both cars and must step them in the
// same order, or the two simulations are not the same simulation.
enum NetCarOwner
{
    NetCar_Host   = 0,
    NetCar_Joiner = 1
};

// --- Session lifetime ------------------------------------------------------
// `now` is DXUTGetTime(), the same monotonic clock Net_Lockstep is given.

// Bind and wait for a joiner on `trackID`, to be raced in `superLeague`. The
// host chooses track, league, seed and dt; the joiner adopts all four.
bool NetGameHost(int trackID, bool superLeague, double now);

// Connect out to `address` (a name or a v4/v6 literal). Blocking DNS, so this
// is a menu call, never a frame-loop one.
bool NetGameJoin(const char* address, double now);

// Drop the session and unlock the sim settings.
void NetGameCancel();

// Pump the transport. Call once per rendered frame. Returns true on the single
// frame the handshake completes, which is the caller's cue to start the race on
// NetGameTrack().
bool NetGamePoll(double now);

// A session exists — listening, connecting, or racing.
bool NetGameSessionActive();

// Connected and simulating. False while still handshaking, and false again once
// the session has failed or desynced.
bool NetGameRacing();

// True if this machine is the host. Fixes which car is which on both peers.
bool NetGameLocalIsHost();

// The car this machine's player drives, and the other one.
inline NetCarOwner NetGameLocalCar()  { return NetGameLocalIsHost() ? NetCar_Host : NetCar_Joiner; }
inline NetCarOwner NetGameRemoteCar() { return NetGameLocalIsHost() ? NetCar_Joiner : NetCar_Host; }

// The agreed track. Valid once connected; before that it is the host's choice
// (host side) or -1 (joiner side, which has not been told yet).
int NetGameTrack();

// The agreed league, on the same terms as NetGameTrack(): the host's own choice
// before a joiner arrives, and the host's choice on both machines afterwards.
// This is a simulation input - see NetSessionConfig::superLeague - so the race
// must be started from this and not from whatever league the local career or
// menu is sitting in.
bool NetGameSuperLeague();

// One line for the waiting screen and the in-race overlay. Never NULL, and
// always safe to print — it says what the session is doing and, on failure,
// why it stopped.
const char* NetGameStatusLine();

// True when the session has ended badly (refused, timed out, desynced) and the
// status line is the explanation rather than progress.
bool NetGameFailed();

// True if a pause should be refused because a network race is running, printing
// a one-line reason naming `what` when it refuses (so a dead key explains
// itself). Pausing in lockstep is not a local matter: this machine would simply
// stop submitting inputs, and the other player's race would freeze with it,
// with nothing on their screen to say why. There is no "pause" in a two-player
// race — only a quit, which at least tells the other end.
//
// Shaped like SimSettingLocked in Physics_FloatV2.h, and used the same way: the
// pause keys live in two separate switch statements hundreds of lines apart
// (the DirectX WM_KEYDOWN one and the SDL one), so every gate has to be done
// twice.
bool NetGamePauseBlocked(const char* what);

// --- The race clock --------------------------------------------------------
// The step counter is the shared clock: both peers start at 0 on the first
// simulated step and advance together. It is not wall time and must never be
// derived from it.

// Called as the race is entered. Zeroes the step counter, seeds the shared RNG
// from the agreed seed, and locks the sim settings for the session.
void NetGameRaceBegun();

// Called when the race ends or is abandoned.
void NetGameRaceEnded();

uint32_t NetGameStep();
void     NetGameAdvanceStep();

// --- The two-driver league -------------------------------------------------
// A session is a season: the two players race a fixture, land on a table, and
// the host picks the next circuit. Each track is used once, so the season is
// kNetSeasonRaces long and ends when they run out.
//
// Nothing here is a simulation input, and nothing here may become one. The
// table is scored *after* a race from results both peers computed identically,
// and the next track is agreed *between* races over the control channel. If
// any of it were ever read during a step, a lost or late control packet would
// become a desync.
//
// Scoring is the Amiga's: two points for the win, one for the fastest lap
// (StuntCarRacer.s, the RESULT screen). The wreck column is this port's own -
// the Amiga had nowhere to record it, because its opponent could not wreck.

const int kNetSeasonRaces = 8;      // one per circuit

// Step numbers each race starts on: race N begins at N * this. Comfortably
// longer than any race can run (a million steps is over four hours at 60Hz), so
// two races can never overlap on the step line and the whole season stays well
// inside a uint32. See NetBeginRaceAt for why the counter does not restart.
const uint32_t kNetRaceStepBase = 1000000u;

struct NetLeagueDriver
{
    char name[16];      // as typed on the NAME? screen; the Amiga's slot size
    int  raced;
    int  won;
    int  lost;
    int  wrecked;
    int  bestLaps;      // races in which this driver set the fastest lap
    int  points;
};

// Clear the table and start a fresh season. Called as a session is established.
void NetGameLeagueReset();

// The standings. Indexed by role, so both machines agree which row is which.
const NetLeagueDriver& NetGameLeagueDriver(NetCarOwner who);

// Score the race just finished. Both peers call this with values they each
// derived from the same simulation, so neither has to tell the other.
// `bestLap` is NetCar_Host / NetCar_Joiner / -1 for "nobody set one".
void NetGameLeagueScoreRace(bool hostWon, bool hostWrecked, bool joinerWrecked, int bestLap);

// Races completed, and whether that is the lot.
int  NetGameRacesRun();
bool NetGameSeasonOver();

// True once `track` has been raced this season, so the host's chooser can skip
// it and the table can grey it out.
bool NetGameTrackUsed(int track);

// The host's choice of the next circuit, and how it gets to the joiner. The
// host calls Propose; both ends read NextTrack, which is -1 until agreed.
void NetGameProposeNextTrack(int track, double now);
int  NetGameNextTrack();

// Abandon the race in progress, conceding it. Tells the peer, which scores the
// win and returns to the table - the alternative is the quitter vanishing and
// the other player stalling until the session times out.
void NetGameForfeitRace(double now);

// True on the single frame a peer's forfeit arrives, so the caller can unwind
// its own race. The race is already scored by the time this returns true.
bool NetGamePeerForfeited();

// The other player's name, or "" until the exchange completes.
const char* NetGameRemoteName();

// This machine's driver name, to be sent to the peer once connected. Set from
// the menus rather than read from League.h here: this file is included beside
// dx_linux.h and must keep to <cstdint>.
void NetGameSetLocalName(const char* name);

} // namespace scr

#endif // NET_GAME_H
