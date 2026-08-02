// Physics_FloatV2 — see header for context.
//
// Ported from reference/floatv2-decompiled/PhysicsFloatV2.cs.
// Sections below are ordered to match the C# source; keep C# line/method
// comments in place while porting so cross-referencing stays cheap.

#include "Physics_FloatV2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace scr {

namespace {

// --- Common helpers -------------------------------------------------------
// Amiga integer angle convention: 65536 == one full turn (2π).
constexpr double kAngleToRadians = 2.0 * 3.14159265358979323846 / 65536.0;

inline double SinF(double angle) { return std::sin(angle * kAngleToRadians); }
inline double CosF(double angle) { return std::cos(angle * kAngleToRadians); }

// Per-tick multiplicative decay used all over the physics (spring damping,
// friction, etc.). At dtRatio=1 this is a plain scale by 119/128 (≈0.930).
// At other rates it compounds so the per-second decay stays constant.
inline double ReduceValue(double value, double dtRatio = 1.0) {
    return value * std::pow(119.0 / 128.0, dtRatio);
}

// Normalise an angle back into the signed 16-bit Amiga range [-32768, 32767].
inline double WrapAngle(double a) {
    a = std::fmod(a, 65536.0);
    if (a >  32767.0) a -= 65536.0;
    else if (a < -32768.0) a += 65536.0;
    return a;
}

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

// Scratch matrix / trig cache passed between sub-steps. Indices match the C#
// `sc` array 1:1 so ports of later sub-steps read the same slots. Kept as a
// std::array (not new'd per tick) — one of the port's small perf wins over
// the C# reference.
using ScArray = std::array<double, 36>;

// MakeRotationMatrix — PhysicsFloatV2.cs:896
// Fills the scratch trig cache from the current car angles + section angle.
// Layout is inherited from the 68k original; treat the numeric slot indices
// as opaque and keep them consistent across sub-steps.
void MakeRotationMatrix(const PhysicsStateF& s, ScArray& sc) {
    // Seed slots that later get multiplied by X/Z trig factors.
    double sinY = SinF(s.YAngle);
    sc[2] = sc[6] = sc[7] = sc[10] = sc[11] = sinY;
    double cosY = CosF(s.YAngle);
    sc[3] = sc[8] = sc[9] = sc[12] = sc[13] = cosY;

    double sectionRelAngle = s.YAngle - s.SectionYAngle;
    double sinSec = SinF(sectionRelAngle);
    sc[26] = sc[33] = sc[34] = sinSec;
    double cosSec = CosF(sectionRelAngle);
    sc[28] = sc[31] = sc[35] = cosSec;

    double sinX = SinF(s.XAngle);
    sc[4] = sinX;
    double cosX = CosF(s.XAngle);
    sc[5] = sc[14] = sc[15] = cosX;

    double cosZ = CosF(s.ZAngle);
    sc[17] = cosZ;
    double sinZ = SinF(s.ZAngle);
    sc[16] = sinZ;

    // Fold sinX into Y-trig columns.
    for (int i = 6; i <= 9; ++i) sc[i] *= sinX;
    sc[26] *= sinX;
    sc[28] *= sinX;

    // Copy pre-fold Y sin/cos so later steps can reach them.
    sc[0] = sc[6];
    sc[1] = sc[8];

    // Fold cosX into remaining Y-trig slots.
    sc[2]  *= cosX;
    sc[3]  *= cosX;
    sc[34] *= cosX;
    sc[35] *= cosX;

    // Fold sinZ.
    sc[6]  *= sinZ;
    sc[8]  *= sinZ;
    sc[10] *= sinZ;
    sc[12] *= sinZ;
    sc[14] *= sinZ;
    sc[26] *= sinZ;
    sc[28] *= sinZ;

    // Fold cosZ.
    sc[7]  *= cosZ;
    sc[9]  *= cosZ;
    sc[11] *= cosZ;
    sc[13] *= cosZ;
    sc[15] *= cosZ;
    sc[31] *= cosZ;
    sc[33] *= cosZ;

    // Composite basis vectors used by later steps.
    sc[20] = sc[12] - sc[7];
    sc[21] = 0.0 - sc[9] - sc[10];
    sc[22] = sc[13] + sc[6];
    sc[23] = sc[8]  - sc[11];
    sc[24] = 0.0 - sc[14];
    sc[18] = 0.0 - sinZ;
}

// CalculateWheelXZOffsets — PhysicsFloatV2.cs:941
// Given the section-relative rotation trigs in sc[], compute XZ offsets of
// each wheel from the car centre in world-oriented coordinates. Constants
// are the Amiga's wheelbase/track values expressed in the physics units
// (512 ≈ half-track, 1024 ≈ half-wheelbase; the odd decimals come from the
// (n - 1/64) rounding baked into the original fixed-point tables).
struct WheelXZ { double flX, flZ, frX, frZ, rX, rZ; };

WheelXZ CalculateWheelXZOffsets(const ScArray& sc) {
    double halfTrackX  = (sc[31] - sc[26]) * 511.984375;
    double halfTrackZ  = (sc[28] - sc[33]) * 511.984375;
    double halfBaseX   = sc[34] * 1023.96875;
    double halfBaseZ   = sc[35] * 1023.96875;
    return WheelXZ{
        /* flX */ halfBaseX - halfTrackX,
        /* flZ */ halfBaseZ - halfTrackZ,
        /* frX */ halfBaseX + halfTrackX,
        /* frZ */ halfBaseZ + halfTrackZ,
        /* rX  */ -halfBaseX,
        /* rZ  */ -halfBaseZ,
    };
}

// CalculateActualWheelHeights — PhysicsFloatV2.cs:1159
// Actual (car-body) wheel heights from the car centre. Rear wheel sits
// along the car's forward axis (sc[4]=sinX); front pair are offset laterally
// by the roll-adjusted half-track (sc[16]=sinZ). Divide by 256 undoes an
// upstream fixed-point scale.
struct WheelActualH { double fl, fr, r; };
WheelActualH CalculateActualWheelHeights(const PhysicsStateF& s, const ScArray& sc) {
    double lateral   = sc[16] * 262136.0;
    double longAxis  = sc[4]  * 524272.0;
    double frontY    = s.WorldY + longAxis;
    return WheelActualH{
        /* fl */ (frontY + lateral) / 256.0,
        /* fr */ (frontY - lateral) / 256.0,
        /* r  */ (s.WorldY - longAxis) / 256.0,
    };
}

// CalculateXZSpeeds — PhysicsFloatV2.cs:1170
// Project world-space velocity into car-local X (sideways) and Z (forward)
// via the rotation basis vectors already baked into sc[].
struct LocalSpeed { double x, z; };
LocalSpeed CalculateXZSpeeds(const PhysicsStateF& s, const ScArray& sc) {
    return LocalSpeed{
        /* x */ sc[22] * s.WorldXSpeed + sc[24] * s.WorldYSpeed + sc[23] * s.WorldZSpeed,
        /* z */ sc[2]  * s.WorldXSpeed + sc[4]  * s.WorldYSpeed + sc[3]  * s.WorldZSpeed,
    };
}

// SetWheelRotationSpeed — PhysicsFloatV2.cs:1176
// Wheel-spin animation speed. In-air wheels decay via ReduceValue-style
// exponential (with a heavier 0.75 factor); on-road speed is a piecewise
// function of forward speed with a clamped upper bound.
void SetWheelRotationSpeed(PhysicsStateF& s, double zSpeed, double dtRatio) {
    double absZ = std::fabs(zSpeed);
    s.PosPlayersZSpeed = absZ;
    if (s.TouchingRoad == 0) {
        s.WheelRotationSpeed *= std::pow(0.75, dtRatio);
        return;
    }
    if (absZ < 2048.0) {
        s.WheelRotationSpeed = absZ * 8.0;
        return;
    }
    double v = absZ * 2.0 + 12288.0;
    if (v > 65535.0) v = 65280.0;
    s.WheelRotationSpeed = v;
}

// CalculateGravityAcceleration — PhysicsFloatV2.cs:1197
// Rotates the world-down gravity vector into car-local space. The 317
// scalar is the Amiga's per-step gravity in physics units.
struct GravityXYZ { double x, y, z; };
GravityXYZ CalculateGravityAcceleration(const ScArray& sc) {
    return GravityXYZ{
        /* x */  317.0 * sc[14],
        /* y */ -317.0 * sc[15],
        /* z */ -317.0 * sc[4],
    };
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
    ScArray sc{};
    MakeRotationMatrix(state, sc);
    WheelXZ wheels = CalculateWheelXZOffsets(sc);
    (void)wheels;   // will feed CalculateRoadWheelHeights (sub-step 4)
    WheelActualH actualH = CalculateActualWheelHeights(state, sc);
    LocalSpeed   local   = CalculateXZSpeeds(state, sc);
    state.PlayersZSpeed  = local.z;
    SetWheelRotationSpeed(state, local.z, dtRatio);
    GravityXYZ grav = CalculateGravityAcceleration(sc);
    (void)actualH; (void)grav;   // consumers land with sub-steps 9-11
    // TODO: port remaining sub-steps 4, 9-17 (see plan above). Until then,
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
