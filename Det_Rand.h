// Det_Rand.h — deterministic pseudo-random numbers for the simulation path.
//
// Why this exists: the C library's rand() is NOT reproducible across the three
// targets. glibc returns 0..2^31-1 from an additive-feedback generator, MinGW's
// msvcrt returns 0..32767 from a 32-bit LCG, and Apple's libc is different again
// (and its rand() is arc4random-derived in some configurations). Any lockstep
// session would desync on the first call.
//
// Several rand() calls sit squarely inside the simulation:
//   Car_Behaviour.cpp      car_on_chains_countdown  (feeds leftRightValue in Tick)
//   Opponent_Behaviour.cpp opponent drop height, max speed, wheelies, steering
// Those now call SCR_Rand(). Render-only users (spark positions, engine sound
// fluctuation) deliberately still call rand(), because pulling them onto the
// shared stream would make the sim depend on how often frames are drawn.
//
// The generator is xorshift32 (Marsaglia 2003): three shifts and three XORs on a
// uint32_t, so it is bit-identical anywhere uint32_t is. Period 2^32-1. Quality
// is far beyond what the callers need — every one of them immediately masks down
// to 4-8 bits.

#pragma once

#include <cstdint>

namespace scr {
namespace det {

// The one simulation RNG stream. Everything reached from a physics step draws
// from this, so a trace can be replayed by seeding it and nothing else.
inline uint32_t& RandState()
{
    static uint32_t state = 0x2545F491u;   // any non-zero value
    return state;
}

inline void SeedRand(uint32_t seed)
{
    // xorshift32 dies on zero, so fold it away rather than trusting the caller.
    RandState() = seed ? seed : 0x9E3779B9u;
}

// Returns 0..0x7FFFFFFF, matching the widest rand() range any target offers, so
// existing "& 0xf" / "% n" call sites keep their distributions.
inline int32_t Rand()
{
    uint32_t x = RandState();
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    RandState() = x;
    return static_cast<int32_t>(x & 0x7FFFFFFFu);
}

} // namespace det
} // namespace scr

// Short spelling for the call sites, which are plain C-style code.
#define SCR_Rand() (::scr::det::Rand())
