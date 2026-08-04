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

// Bind and wait for a joiner on `trackID`. The host chooses track, seed and dt;
// the joiner adopts all three.
bool NetGameHost(int trackID, double now);

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

// One line for the waiting screen and the in-race overlay. Never NULL, and
// always safe to print — it says what the session is doing and, on failure,
// why it stopped.
const char* NetGameStatusLine();

// True when the session has ended badly (refused, timed out, desynced) and the
// status line is the explanation rather than progress.
bool NetGameFailed();

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

} // namespace scr

#endif // NET_GAME_H
