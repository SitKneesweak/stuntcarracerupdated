// Sim_Trace.h — per-step physics state checksum, for proving determinism.
//
// The determinism work (see Det_Math.h, -ffp-contract=off) makes the physics
// *should*-be-reproducible. This turns that into a measurement: every physics
// step writes its step number and a hash of the full PhysicsStateF to a log
// file, and the logs from two platforms are diffed. The first differing line is
// the first divergent step; re-running with verbose tracing then dumps every
// field of that step so the culprit can be named.
//
// Two things are needed for the logs to be comparable, and this module provides
// both:
//
//  1. A *bit-pattern* hash. Comparing printed doubles would hide sub-ulp drift,
//     which is exactly the drift that compounds into a desync.
//
//  2. A repeatable input source. Two hand-driven runs never match, so trace mode
//     ignores the keyboard and feeds the physics a scripted input stream derived
//     from the step number alone. It also pins the timestep (one step per render
//     frame, at a fixed dt) so the wall-clock accumulator in OnFrameMove cannot
//     make the step count depend on frame rate, and seeds Det_Rand.
//
// The log goes to a *file*, not stdout, because the Windows build is run on a
// machine with no shared terminal — a file can be copied back and diffed.
//
// Usage:
//   stuntcarracer --simtrace              trace 6000 steps (100s at 60Hz) and quit
//   stuntcarracer --simtrace 10000        trace a specific number of steps
//   stuntcarracer --simtrace-verbose      dump every field, not just the hash
//   stuntcarracer --simtrace-track 3      pick the track (default 0, Little Ramp)
//   stuntcarracer --simtrace-digest       ~60 pasteable lines instead of 6000
//   stuntcarracer --simtrace-solo         race with no AI opponent
//   stuntcarracer --simtrace-window N C   log only steps N..N+C, verbosely
//   stuntcarracer --simtrace-out FILE     log path (default simtrace.log)
//
// Comparing two machines, cheapest first:
//   1. Compare the single "# DIGEST <steps> <hash>" line. Equal => every bit of
//      every step agreed, and there is nothing else to check.
//   2. If it differs, compare the "#chk <step> <digest>" lines to find the first
//      differing 100-step window.
//   3. Re-run both with --simtrace to get per-step hashes, and compare that
//      window's 100 lines to find the exact first differing step.
//   4. Re-run both with --simtrace-window <step> 1 and compare the single
//      verbose line, which names the field.

#pragma once

#include <cstdint>

namespace scr {

struct PhysicsStateF;

// True once --simtrace has been given. While set, the game auto-starts a race,
// takes its input from the script below, and runs a fixed timestep.
extern bool  gSimTraceEnabled;
// Dump every field of every step as well as the hash. Much larger files, but it
// names the diverging field directly instead of just the step.
extern bool  gSimTraceVerbose;
// Write only the header, the 100-step checkpoints and the final digest -- about
// 60 lines for a default run, small enough to paste by hand off a machine with
// no shared terminal. That is the intended way to run this on the PC.
extern bool  gSimTraceDigestOnly;
// Log only steps [first, first+count) and log them verbosely. The simulation is
// unaffected -- every step still runs and still feeds the digest -- so this only
// changes how much has to be moved between machines.
extern long  gSimTraceWindowFirst;
extern long  gSimTraceWindowCount;
// Steps to record before quitting.
extern long  gSimTraceMaxSteps;
// Track to race on, and the seed handed to Det_Rand at race start.
extern int   gSimTraceTrack;
// Race solo (no AI opponent). The opponent is not hashed but does perturb the
// player via car-to-car impulses, so this separates a player-side divergence
// from one that merely leaks in from the opponent.
extern bool  gSimTraceSolo;
extern uint32_t gSimTraceSeed;

// Parse one command-line argument. Returns the number of argv entries consumed
// (0 if this argument is not ours).
int  SimTrace_ParseArg(int argc, char** argv, int i);

// Open the log and write its header. Called once, at race start.
void SimTrace_Begin();

// Record one physics step. Called from the tail of PhysicsStepF_Tick, so the
// state hashed is the sim's own, before the adapter round-trips it through the
// legacy fixed-point globals.
void SimTrace_RecordStep(const PhysicsStateF& s);

// True once gSimTraceMaxSteps steps have been recorded, so the main loop can
// close the log and exit.
bool SimTrace_Finished();
void SimTrace_End();

// Scripted controller input for the next step, as a KEY_P1_* bitmask. Depends
// only on the step counter, so it is identical on every platform.
uint32_t SimTrace_ScriptedInput();

} // namespace scr
