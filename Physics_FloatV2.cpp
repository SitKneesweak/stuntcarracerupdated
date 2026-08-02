// Physics_FloatV2 — see header for context.
//
// Ported from reference/floatv2-decompiled/PhysicsFloatV2.cs.
// Sections below are ordered to match the C# source; keep C# line/method
// comments in place while porting so cross-referencing stays cheap.

#include "Physics_FloatV2.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace scr {

namespace {

// PhysicsInput packed as Amiga-style bitfield: bit 0 = Accel, 1 = Brake,
// 2 = Left, 3 = Right, 4 = Boost. Matches PhysicsInput.ToByte() in the
// reference (Physics.cs:1283).
inline uint8_t InputToByte(const PhysicsInput& in) {
    uint8_t b = 0;
    if (in.Accelerate) b |= 1;
    if (in.Brake)      b |= 2;
    if (in.Left)       b |= 4;
    if (in.Right)      b |= 8;
    if (in.Boost)      b |= 16;
    return b;
}

// Signed narrow: mirrors C# `(short)Math.Max(Math.Min(Math.Round(x), 32767), -32768)`
inline int16_t SaturateToShort(double x) {
    double r = std::round(x);
    if (r >  32767.0) r =  32767.0;
    if (r < -32768.0) r = -32768.0;
    return static_cast<int16_t>(r);
}

// Amiga BCD subtract-1 (from PhysicsFloatV2.cs:1812). Reserve values are
// stored as packed BCD; the 10s and units digits decrement with borrow.
inline uint8_t BcdSubtract1(uint8_t bcd) {
    int lo = 9 + (bcd & 0xF);
    int borrow = 0;
    if (lo >= 10) { lo -= 10; borrow = 1; }
    int hi = 9 + ((bcd >> 4) & 0xF) + borrow;
    if (hi >= 10) hi -= 10;
    return static_cast<uint8_t>((hi << 4) | lo);
}

// BoostPower — PhysicsFloatV2.cs:1776
void BoostPower(PhysicsStateF& s, const PhysicsInput& input, uint8_t boostFlag, double dtRatio) {
    uint8_t wreckByte = static_cast<uint8_t>((s.WreckWheelHeightReduction >> 8) & 0xFF);
    if ((boostFlag | wreckByte) != 0) {
        s.BoostActivated = 0;
        return;
    }
    if (static_cast<int8_t>(s.Accelerating) >= 0 && (InputToByte(input) & 3) == 0) {
        s.BoostActivated = 0;
        return;
    }
    if (s.BoostReserve == 0) {
        s.BoostActivated = 0;
        return;
    }
    if (static_cast<int8_t>(s.FourteenFramesElapsed) >= 0) {
        s.BoostUnit -= dtRatio;
        if (s.BoostUnit < 0.0) {
            s.BoostUnit += static_cast<int>(s.BoostUnitValue);
            uint8_t next = BcdSubtract1(s.BoostReserve);
            if (next >= s.BoostMaxUnits) next = s.BoostMaxUnits;
            s.BoostReserve = next;
        }
    }
    s.BoostActivated = 128;
    // Engine acceleration doubles while boosting (16-bit wrap by << 1).
    s.EngineZAcceleration = static_cast<int16_t>(static_cast<int16_t>(s.EngineZAcceleration) << 1);
}

// ComputeEngineAcceleration — PhysicsFloatV2.cs:1746
void ComputeEngineAcceleration(PhysicsStateF& s, const PhysicsInput& input, double dtRatio) {
    uint8_t boostFlag = input.Boost ? 0 : 16;
    int16_t engineAccel = 0;

    // Take the high byte of Z-speed, saturated to a signed 16-bit range. The
    // original assembly used a fixed-point speed; the high byte drops LSBs.
    uint8_t speedHi = static_cast<uint8_t>(SaturateToShort(s.PlayersZSpeed)) >> 0;
    // Actually we want the *high* byte of the 16-bit value:
    speedHi = static_cast<uint8_t>(static_cast<uint16_t>(SaturateToShort(s.PlayersZSpeed)) >> 8);

    // Only apply engine torque if the car is going slow-ish (<120 in high-byte
    // units) or reversing (sign bit set), and we're not chained/wrecked.
    bool speedOk = (static_cast<int8_t>(speedHi) < 0) || (speedHi < 120);
    bool notChained = (s.CarOnChainsCountdown == 0);
    bool notWrecked = (static_cast<uint8_t>((s.WreckWheelHeightReduction >> 8) & 0xFF)) == 0;

    if (speedOk && notChained && notWrecked) {
        uint8_t accelBrakeBits = static_cast<uint8_t>(InputToByte(input) & 3);
        // Endianness swap of EnginePower (Amiga stored bytes reversed relative
        // to little-endian). Matches ((byte)(ep & 0xFF) << 8) | (byte)(ep >> 8).
        int16_t swappedEnginePower = static_cast<int16_t>(
            (static_cast<uint16_t>(s.EnginePower & 0xFF) << 8) |
            static_cast<uint16_t>((s.EnginePower >> 8) & 0xFF));

        if (accelBrakeBits == 1) {           // accel only
            engineAccel = swappedEnginePower;
            s.Accelerating = 128;
        } else if (accelBrakeBits > 1) {     // brake (or both) wins
            engineAccel = -240;
            s.Accelerating = 0;
        } else if (static_cast<int8_t>(s.Accelerating) < 0) {
            // Sticky-accelerate: once bit-7 is set on Accelerating, keep
            // applying engine power even without the button (matches Amiga
            // "auto-accelerate after first press" behaviour).
            engineAccel = swappedEnginePower;
            s.Accelerating = 128;
        }
    }

    s.EngineZAcceleration = engineAccel;
    BoostPower(s, input, boostFlag, dtRatio);
}

} // anonymous namespace

bool gUseFloatV2Physics = false;   // flipped by F-key toggle once port is landable

// --- Tuning constants (from PhysicsStepF in PhysicsFloatV2.cs) --------------
namespace {
    constexpr double BaseDt = 0.1;
    // Additional constants will be inlined here as we port each sub-step.
}

// --- Sub-steps -------------------------------------------------------------
// Each helper below corresponds one-to-one with a static in PhysicsStepF.
// PORT PLAN (translate in this order — each is self-contained enough to test):
//   1. ComputeEngineAcceleration
//   2. MakeRotationMatrix
//   3. CalculateWheelXZOffsets
//   4. CalculateRoadWheelHeights   <-- depends on Track adapter
//   5. CalculateActualWheelHeights
//   6. CalculateXZSpeeds
//   7. SetWheelRotationSpeed
//   8. CalculateGravityAcceleration
//   9. CarCollisionDetection       <-- ProcessWheel inside
//  10. CalculateCarCollisionAcceleration
//  11. CalculateTotalAcceleration
//  12. CalculateSteering
//  13. CalculateWorldAcceleration
//  14. ReduceWorldAcceleration
//  15. CalculateXZRotationAcceleration
//  16. IntegrateMotion
//  17. UpdateBoost / UpdateDamage / UpdateWheelHeightReduction / etc.

void PhysicsStepF_Tick(PhysicsStateF& state, const PhysicsInput& input, double dt)
{
    const double dtRatio = dt / BaseDt;
    ComputeEngineAcceleration(state, input, dtRatio);
    // TODO: port remaining sub-steps 2-17 (see plan above). Until then,
    // callers should keep gUseFloatV2Physics == false so the legacy
    // CarBehaviour() path stays authoritative.
}

// --- Legacy <-> FloatV2 adapters -------------------------------------------
// player_x, player_y, ... are extern longs in Car_Behaviour.cpp using the
// Amiga fixed-point convention (Y is negated relative to render Y; positions
// scaled by 1 << LOG_PRECISION). We convert to plain metres-ish doubles here.

void CopyLegacyToFloatV2(PhysicsStateF& /*out*/)
{
    // TODO: mirror ResetPlayer() + current per-tick state into FloatV2 struct.
}

void CopyFloatV2ToLegacy(const PhysicsStateF& /*in*/)
{
    // TODO: write FloatV2 state back into player_x/y/z, angles, wheel data,
    // damage counters, etc. so the renderer and HUD keep working unchanged.
}

} // namespace scr
