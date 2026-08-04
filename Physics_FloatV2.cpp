// Physics_FloatV2 — see header for context.
//
// Ported from reference/floatv2-decompiled/PhysicsFloatV2.cs.
// Sections below are ordered to match the C# source; keep C# line/method
// comments in place while porting so cross-referencing stays cheap.

#include "Physics_FloatV2.h"
#include "Track_FloatV2.h"
#include "Det_Math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace scr {

namespace {

// --- Common helpers -------------------------------------------------------
// Amiga integer angle convention: 65536 == one full turn (2π).
//
// These go through Det_Math rather than libm: sin/cos/pow are not required to
// be correctly rounded and genuinely differ between glibc, Apple's libm and
// MSVCRT, which would desync lockstep multiplayer. See Det_Math.h.
inline double SinF(double angle) { return det::SinUnits(angle); }
inline double CosF(double angle) { return det::CosUnits(angle); }

// Per-tick multiplicative decay used all over the physics (spring damping,
// friction, etc.). At dtRatio=1 this is a plain scale by 119/128 (≈0.930).
// At other rates it compounds so the per-second decay stays constant.
inline double ReduceValue(double value, double dtRatio = 1.0) {
    return value * det::PowFromLog2(det::kLog2_119_128, dtRatio);
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

// --- Sub-step 4: road heights under each wheel ----------------------------
// PhysicsFloatV2.cs:955-1157, with the track data reached through the
// C#-shaped view in Track_FloatV2.h. Everything here is Amiga integer maths
// (8.8 fixed point along the section, 0..255 across it) even though the
// wheel offsets arriving from sub-step 3 are doubles.

// SignalOffRoad — PhysicsFloatV2.cs:1153
// Shifts a 1 into the top of AtSideByte (a 3-frame history the collision code
// reads) and reports the minimum "ground" height.
int SignalOffRoad(PhysicsStateF& s) {
    s.AtSideByte = static_cast<uint8_t>((s.AtSideByte >> 1) | 0x80);
    return 4096;
}

// HandleOffRoad — PhysicsFloatV2.cs:1122
// Just past either road edge there is a narrow (48 unit) sloped verge the car
// can ride on; beyond that, or if the verge has fallen too far below the road,
// the wheel is off the track entirely.
int HandleOffRoad(PhysicsStateF& s, int16_t wheelRoadXPos, int roadHeight, uint8_t plus180) {
    int distPastEdge;
    if (wheelRoadXPos < 0) {
        distPastEdge = -wheelRoadXPos;
    } else {
        distPastEdge = wheelRoadXPos - 384;
        if (distPastEdge < 0) distPastEdge = -distPastEdge;
    }
    if (distPastEdge > 48) return SignalOffRoad(s);

    distPastEdge = (distPastEdge & 0xFF) << 4;
    int height = roadHeight - distPastEdge - 256;
    if (height < 4096) return SignalOffRoad(s);

    uint8_t side = static_cast<uint8_t>(static_cast<uint8_t>(wheelRoadXPos) ^ plus180);
    s.WhichSideByte = static_cast<uint8_t>((side & 0x80) ? 128 : 64);
    return height;
}

// HandleSectionCrossing — PhysicsFloatV2.cs:1059
// A wheel can sit in the section ahead of or behind the car's own. Step to
// that section and re-express the along/across position in its coordinate
// frame — which may need mirroring if the two sections run opposite ways.
void HandleSectionCrossing(const FV2Track& t, int& currentSection, uint8_t plus180,
                           int16_t surfaceZ, int& surfaceX, int16_t& newSurfaceZ) {
    uint8_t zHi = static_cast<uint8_t>(surfaceZ >> 8);
    bool forwards = static_cast<int8_t>(static_cast<uint8_t>(zHi ^ plus180)) >= 0;

    if (forwards) {
        if (++currentSection >= t.SectionCount) currentSection = 0;
    } else {
        if (--currentSection < 0) currentSection = t.SectionCount - 1;
    }

    const FV2RoadSection& sec   = t.Sections[currentSection];
    const FV2RoadPiece&   piece = FV2_GetPiece(sec.PieceIndex);
    uint8_t newPlus180 = static_cast<uint8_t>((sec.Angle & 0x10) << 3);

    bool atFarEnd = forwards ? ((newPlus180 & 0x80) != 0) : ((newPlus180 & 0x80) == 0);

    int z = surfaceZ & 0xFF;
    int x = surfaceX & 0xFF;

    int coordIdx;
    bool mirror;
    if (atFarEnd) {
        coordIdx = piece.CoordCount - 2;
        mirror   = static_cast<int8_t>(zHi) >= 0;
    } else {
        coordIdx = 0;
        mirror   = static_cast<int8_t>(zHi) < 0;
    }

    if (mirror) {
        z = -z & 0xFF; if (z == 0) z = 255;
        x = -x & 0xFF; if (x == 0) x = 255;
    }

    newSurfaceZ = static_cast<int16_t>((coordIdx << 8) | z);
    surfaceX    = x;
}

// ProcessOneWheel — PhysicsFloatV2.cs:965
// Converts one wheel's XZ offset into a road height, following the wheel into
// a neighbouring section if it has crossed out of the car's own.
double ProcessOneWheel(PhysicsStateF& s, const FV2Track& t, int& currentSection,
                       double wheelXOff, double wheelZOff, double& storedHeight,
                       double dtRatio, bool isRearWheel = false) {
    currentSection = s.RoadSection;

    const FV2RoadSection* sec   = &t.Sections[currentSection];
    const FV2RoadPiece*   piece = &FV2_GetPiece(sec->PieceIndex);
    uint8_t plus180 = static_cast<uint8_t>((sec->Angle & 0x10) << 3);

    // Across the road: wheel offset (in 1/16ths) plus the car's own position.
    int16_t roadX = static_cast<int16_t>(
        static_cast<int>(std::floor(wheelXOff / 16.0) + s.PlayersRoadXPosition));

    bool     offRoad = false;
    int16_t  wheelRoadXPos = 0;
    int      surfaceX;
    if (static_cast<uint16_t>(roadX) >= 384) {
        // Unsigned compare: catches both edges at once (negative wraps high).
        offRoad = true;
        wheelRoadXPos = roadX;
        surfaceX = (roadX >= 0) ? 255 : 0;
    } else {
        int16_t absX = (roadX < 0) ? static_cast<int16_t>(-roadX) : roadX;
        int width = (piece->WidthReduction << 7) & 0x7FFF;
        surfaceX = static_cast<int>((static_cast<uint32_t>(absX * width) << 1) >> 16);
        if (surfaceX >= 256) surfaceX = 255;
    }

    if (isRearWheel) {
        uint8_t rearX = static_cast<uint8_t>(surfaceX);
        if (static_cast<int8_t>(plus180) < 0) rearX ^= 0xFF;
        s.RearWheelSurfaceXPosition = rearX;
    }

    // Along the road: 8.8 fixed point, high byte selecting the segment.
    int16_t zOff = static_cast<int16_t>(static_cast<int>(std::floor(wheelZOff / 8.0)));
    int length = (piece->LengthReduction << 7) & 0x7FFF;
    int16_t surfaceZ = static_cast<int16_t>(
        static_cast<int16_t>((static_cast<uint32_t>(zOff * length) << 1) >> 16) +
        static_cast<int16_t>(static_cast<int>(s.NormalDistanceIntoSection)));

    uint8_t segIdx = static_cast<uint8_t>(static_cast<uint8_t>(surfaceZ >> 8) << 1);
    uint8_t lastSeg = static_cast<uint8_t>(piece->CoordCount * 2 - 2);
    if (static_cast<int8_t>(segIdx) < 0 || segIdx >= lastSeg) {
        HandleSectionCrossing(t, currentSection, plus180, surfaceZ, surfaceX, surfaceZ);
        sec     = &t.Sections[currentSection];
        piece   = &FV2_GetPiece(sec->PieceIndex);
        plus180 = static_cast<uint8_t>((sec->Angle & 0x10) << 3);
    }

    int height = FV2_GetRoadHeight(t, currentSection,
                                   static_cast<uint16_t>(surfaceZ), surfaceX & 0xFF);
    if (offRoad) height = HandleOffRoad(s, wheelRoadXPos, height, plus180);

    // Above a certain speed, or at a steep pitch, take the new height as-is.
    // Otherwise ease towards it — this is the road "cushion" that stops the
    // car chattering over segment boundaries at low speed.
    double previous = storedHeight;
    // Debug: capture the raw table result for the front-left wheel, i.e. the
    // last wheel CalculateRoadWheelHeights processes (see call order below).
    const bool dbgWheel = !isRearWheel;
    if (dbgWheel) {
        gDbgRawRoadFL = height;
        gDbgPosZSpeed = s.PosPlayersZSpeed;
        gDbgSurfZ     = static_cast<double>(static_cast<uint16_t>(surfaceZ));
        gDbgBlendUsed = 0;
        gDbgFV2Section = currentSection;
        gDbgFV2Seg     = (static_cast<uint16_t>(surfaceZ) >> 8) & 0xff;
        gDbgFV2ZFrac   = static_cast<uint16_t>(surfaceZ) & 0xff;
        gDbgFV2XFrac   = surfaceX & 0xff;
    }
    if (s.PosPlayersZSpeed >= 2560.0) {
        storedHeight = height;
        return height;
    }

    int pitch = static_cast<uint8_t>(static_cast<int16_t>(static_cast<int>(s.XAngle)) >> 8);
    if (static_cast<int8_t>(pitch) < 0) pitch = static_cast<uint8_t>(-static_cast<int8_t>(pitch));
    if (pitch > 5) {
        storedHeight = height;
        return height;
    }

    // half-way per 10Hz step; log2(0.5) is exactly -1
    double blend = 1.0 - det::Exp2(-dtRatio);
    if (dbgWheel) gDbgBlendUsed = 1;
    storedHeight = std::round(previous + blend * (static_cast<double>(height) - previous));
    return storedHeight;
}

// CalculateRoadWheelHeights — PhysicsFloatV2.cs:955
struct WheelRoadH { double fl, fr, r; };
WheelRoadH CalculateRoadWheelHeights(PhysicsStateF& s, const FV2Track& t,
                                     const WheelXZ& w, double dtRatio) {
    int currentSection = s.RoadSection;
    s.AtSideByte = 0;
    s.WhichSideByte = 0;
    // Rear first — it is the wheel that sets RearWheelSurfaceXPosition, and
    // the AtSideByte history is shifted in wheel order.
    WheelRoadH h{};
    h.r  = ProcessOneWheel(s, t, currentSection, w.rX,  w.rZ,  s.RearRoadHeight,       dtRatio, true);
    h.fr = ProcessOneWheel(s, t, currentSection, w.frX, w.frZ, s.FrontRightRoadHeight, dtRatio);
    h.fl = ProcessOneWheel(s, t, currentSection, w.flX, w.flZ, s.FrontLeftRoadHeight,  dtRatio);
    return h;
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
        s.WheelRotationSpeed *= det::PowFromLog2(det::kLog2_3_4, dtRatio);
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

// Fix.Mul — TrackData.cs:24. 68k muls.w with the 1.15 fixed-point shift.
inline int16_t FixMul(int16_t a, int16_t b) {
    return static_cast<int16_t>((static_cast<int32_t>(a) * b << 1) >> 16);
}

// --- Sub-step 9: car/road collision --------------------------------------

// ProcessWheel — PhysicsFloatV2.cs:1264
// One wheel's penetration into the road, plus the damage it takes for it.
// The returned "amount below road" is what the suspension pushes back against.
double ProcessWheel(PhysicsStateF& s, double heightDiff, double& oldDiff,
                    double& amountBelowRoad, uint8_t& damage, double& damageRemainder,
                    double& damageValue, uint8_t& groundedCount, bool& grounded,
                    double dtRatio) {
    double d = heightDiff;
    if (d < 0.0) { if (d < -768.0) d = -768.0; }
    else if (d >= 5120.0) d = 5120.0;

    // Predictive term: extrapolate the approach rate so a fast-closing wheel
    // registers contact this step. The 1.078125 is per-10Hz-step, hence /dtRatio.
    double rate = (d - oldDiff) * (1.078125 / dtRatio);
    double below = rate + d;
    oldDiff = d;

    if (below < 0.0) {
        amountBelowRoad = 0.0;
        grounded = false;
        s.DamagedCount = 0;
        return 0.0;
    }

    amountBelowRoad = below;
    // Touchdown edge, latched: arm below 0x200, fire at 0x400. Equivalent to
    // the Amiga's previous-step test at its own rate, but it still fires when
    // a finer time step walks the wheel through the band gradually - which is
    // why hitting the foot of the big ramp made no sound at 60Hz.
    if (below < 512.0) {
        grounded = false;
    } else if (below >= 1024.0 && !grounded) {
        grounded = true;
        groundedCount++;
    }

    // Impacts beyond the road "cushion" do damage - but not while the crane has
    // the car. On the Amiga the chained car never reached the road, so the case
    // could not arise; the port's crane sets it down on the track before letting
    // go, and being handed a damaged car on the start line is not the deal.
    double impact = below - static_cast<double>(s.RoadCushionValue << 8);
    if (s.CarOnChainsCountdown != 0) {
        s.DamagedCount = 0;
    } else if (impact >= 0.0 && impact >= 1792.0) {
        if (impact > damageValue) damageValue = impact;
        double excess = impact - 1536.0;
        if (static_cast<int8_t>(s.FourteenFramesElapsed) >= 0) {
            s.DamagedCount++;
            // DamagedLimit counts steps, so it scales with the tick rate.
            if (s.DamagedCount < static_cast<int>(static_cast<double>(s.DamagedLimit) / dtRatio)) {
                double clamped = std::round(excess);
                if (clamped > 65535.0) clamped = 65535.0;
                if (clamped < 0.0) clamped = 0.0;
                int hi = (static_cast<int>(clamped) >> 8) & 0xFF;
                int scaled = (hi + (hi >> 1)) & 0xFF;   // x1.5
                // Fractional damage carries between steps so the total dealt
                // per second is rate-independent.
                double amount = static_cast<double>(scaled) * dtRatio + damageRemainder;
                int whole = static_cast<int>(amount);
                damageRemainder = amount - static_cast<double>(whole);
                int total = whole + damage;
                if (total > 255) total = 255;
                damage = static_cast<uint8_t>(total);
                s.Damaged = 128;
            }
        }
    } else {
        s.DamagedCount = 0;
    }

    if (amountBelowRoad >= 4608.0) amountBelowRoad = 4607.0;
    return amountBelowRoad;
}

// CarCollisionDetection — PhysicsFloatV2.cs:1204
struct CollisionResult {
    double flBelow, frBelow, rBelow;
    double overallBelow;    // pitch term: front pair vs rear
    double frontBelow;      // roll term: front left vs front right
    uint8_t groundedCount;
};

CollisionResult CarCollisionDetection(PhysicsStateF& s, const WheelRoadH& road,
                                      const WheelActualH& actual, double dtRatio) {
    CollisionResult r{};
    double damageValue = 0.0;
    // Damaged is a per-step flag: the wheel passes below re-raise it if a wheel
    // is taking damage this step. Clearing it here mirrors the legacy
    // CarCollisionDetection (Car_Behaviour.cpp) - without it the creak sound
    // keeps retriggering long after the impact.
    s.Damaged = 0;
    r.groundedCount = 0;

    double flDiff = road.fl - actual.fl - static_cast<double>(s.WreckWheelHeightReduction);
    r.flBelow = ProcessWheel(s, flDiff, s.OldFrontLeftDiff, s.FrontLeftAmountBelowRoad,
                             s.FrontLeftDamage, s.FrontLeftDamageRemainder,
                             damageValue, r.groundedCount, s.FrontLeftGrounded, dtRatio);
    double frDiff = road.fr - actual.fr - static_cast<double>(s.WreckWheelHeightReduction);
    r.frBelow = ProcessWheel(s, frDiff, s.OldFrontRightDiff, s.FrontRightAmountBelowRoad,
                             s.FrontRightDamage, s.FrontRightDamageRemainder,
                             damageValue, r.groundedCount, s.FrontRightGrounded, dtRatio);
    double rDiff = road.r - actual.r - static_cast<double>(s.WreckWheelHeightReduction);
    r.rBelow = ProcessWheel(s, rDiff, s.OldRearDiff, s.RearAmountBelowRoad,
                            s.RearDamage, s.RearDamageRemainder,
                            damageValue, r.groundedCount, s.RearGrounded, dtRatio);

    s.DamageValue = SaturateToShort(damageValue);
    s.GroundedCount = r.groundedCount;

    double frontAvg = (r.flBelow + r.frBelow) / 2.0;
    double allAvg   = (frontAvg + r.rBelow) / 2.0;

    double roll = r.flBelow - r.frBelow;
    double rollMag = std::fabs(roll * 3.0);
    if (rollMag > 4096.0) rollMag = 4096.0;
    r.frontBelow = (roll < 0.0) ? -rollMag : rollMag;

    r.overallBelow = frontAvg - r.rBelow;

    int16_t avg = SaturateToShort(allAvg);
    s.TouchingRoad = static_cast<uint8_t>(
        static_cast<uint8_t>(avg >> 8) | static_cast<uint8_t>(avg & 0xFF));

    if (s.TouchingRoad == 0 && s.CarOnChainsCountdown == 0) {
        // Airborne: apply a nose-down bias so the car pitches forward in
        // flight. Ski Jump (4) and Roller Coaster (7) use gentler values.
        double bias = -128.0;
        bool apply = true;
        if (s.XAngle >= 0.0) {
            if (s.XAngle >= 4096.0) bias = -256.0;
        } else if (s.RoadID != 7) {
            if (s.RoadID != 4) apply = false;
            else bias = -8.0;
        }
        if (apply) {
            bias -= r.overallBelow;
            if (bias < 0.0) {
                double spin = s.XRotationSpeed / 256.0;
                if (spin >= 0.0 || static_cast<int>(std::floor(spin)) == -1)
                    r.overallBelow = bias;
            }
        }
    }

    s.FrontLeftHeightDifference  = flDiff;
    s.FrontRightHeightDifference = frDiff;
    s.RearHeightDifference       = rDiff;
    return r;
}

// --- Sub-step 10: collision acceleration ----------------------------------

// CalculateInclinationSinCosF — PhysicsFloatV2.cs:1360
// Cheap sin/cos of a road gradient: the Amiga treated 255 as "45 degrees" and
// took sin directly from the gradient, deriving cos from it.
void CalculateInclinationSinCosF(double gradient, double& sinVal, double& cosVal) {
    double mag = std::fabs(gradient);
    sinVal = std::min(mag, 255.0) / 255.0;
    cosVal = std::sqrt(1.0 - sinVal * sinVal);
}

// CalculateCarCollisionAcceleration — PhysicsFloatV2.cs:1335
// Turns suspension compression into a force along the road's local normal,
// so a car on a slope is pushed along it rather than straight up.
struct CollAccel { double x, y, z; };
CollAccel CalculateCarCollisionAcceleration(const PhysicsStateF& s, const CollisionResult& c) {
    double force = ((c.flBelow + c.frBelow) / 2.0 + c.rBelow) / 2.0;

    double pitch = ((s.FrontLeftHeightDifference + s.FrontRightHeightDifference) / 2.0
                    - s.RearHeightDifference) / 16.0;
    double roll  = (s.FrontLeftHeightDifference - s.FrontRightHeightDifference) / 8.0;

    double sinPitch, cosPitch, sinRoll, cosRoll;
    CalculateInclinationSinCosF(pitch, sinPitch, cosPitch);
    CalculateInclinationSinCosF(roll,  sinRoll,  cosRoll);

    double normalY = cosPitch * cosRoll;
    double normalX = cosPitch * sinRoll;

    double rollSign  = (roll < 0.0) ? -1.0 : 1.0;      // zero -> +1
    double pitchSign = (pitch >= 0.0) ? -1.0 : 1.0;    // zero -> -1

    return CollAccel{ force * normalX * rollSign, force * normalY, force * sinPitch * pitchSign };
}

// --- Sub-step 11: total local acceleration --------------------------------

// CalculateTotalAcceleration — PhysicsFloatV2.cs:1367
// Sums gravity, collision and engine in car-local space, with grip limiting:
// engine and lateral forces cannot exceed roughly twice the normal load.
struct TotalAccel { double x, y, z; };
TotalAccel CalculateTotalAcceleration(PhysicsStateF& s, const GravityXYZ& grav,
                                      const CollAccel& coll, double xSpeed, double zSpeed) {
    TotalAccel t{};
    t.y = grav.y + coll.y;

    double engine = s.EngineZAcceleration;
    gDbgZSpeed = zSpeed; gDbgEngineIn = engine;
    gDbgCollY = coll.y; gDbgCollZ = coll.z; gDbgGravZ = grav.z;
    // Rolling resistance, only when engine force and travel agree in sign.
    uint8_t signs = static_cast<uint8_t>(
        static_cast<uint8_t>(static_cast<int16_t>(engine) >> 8) |
        static_cast<uint8_t>(static_cast<int16_t>(zSpeed) >> 8));
    if (static_cast<int8_t>(signs) >= 0 &&
        static_cast<uint8_t>(static_cast<int16_t>(engine) & 0xFF) != 0) {
        engine -= static_cast<double>(signs);
    }

    double grip = (s.TouchingRoad != 0) ? (coll.y * 2.0) : 0.0;
    if (!(std::fabs(engine) < grip)) {
        engine = (engine < 0.0) ? -grip : grip;
    }
    s.EngineZAcceleration = engine;
    gDbgGrip = grip; gDbgEngineOut = engine;
    t.z = engine + coll.z + grav.z;

    double lateral = grav.x + coll.x;
    if (std::fabs(lateral - xSpeed) < grip) {
        // Within grip: sideways speed is cancelled outright (the car "bites").
        t.x = coll.x - xSpeed;
        s.CollisionInAir = 0;
    } else {
        t.x = lateral - ((xSpeed < 0.0) ? -grip : grip);
        s.CollisionInAir = 128;
    }
    return t;
}

// --- Sub-step 12: steering ------------------------------------------------

// ComputeSteeringAcceleration — PhysicsFloatV2.cs:1520
// Steering authority scales with forward speed.
int16_t ComputeSteeringAcceleration(uint8_t factor, int16_t playerZSpeed, int8_t leftRight) {
    int16_t v = FixMul(static_cast<int16_t>(((factor & 0xFF) << 7) & 0x7FFF), playerZSpeed);
    if (leftRight < 0) v = static_cast<int16_t>(-v);
    return static_cast<int16_t>(v >> 3);
}

// ComputeAlignmentAdjustment — PhysicsFloatV2.cs:1553
int16_t ComputeAlignmentAdjustment(uint8_t factor, int16_t playerZSpeed) {
    int16_t speed = playerZSpeed;
    if (speed < 0) speed = static_cast<int16_t>(-speed);
    speed = static_cast<int16_t>(speed + 2560);
    if (speed < 0) speed = 32512;               // overflowed -> clamp
    int16_t v = FixMul(static_cast<int16_t>(((factor & 0xFF) << 7) & 0x7FFF), speed);
    v = static_cast<int16_t>(static_cast<uint16_t>(v) >> 7);
    if (static_cast<uint8_t>(v) == 0) v = 1;
    return v;
}

// AlignCarWithRoad — PhysicsFloatV2.cs:1530
// Steers the car back towards the section's heading when badly misaligned.
void AlignCarWithRoad(PhysicsStateF& s, int16_t posDiffAngle, int16_t differenceAngle,
                      int16_t playerZSpeed, double dtRatio) {
    int16_t adjust = posDiffAngle;
    uint8_t factor = static_cast<uint8_t>(posDiffAngle & 0xFF);
    bool computed = false;

    if (static_cast<uint8_t>(posDiffAngle >> 8) != 0) {
        adjust = static_cast<int16_t>(adjust - 7680);
        if (adjust >= 0) computed = true;       // large error: use it directly
        else factor = 255;
    }
    if (!computed) adjust = ComputeAlignmentAdjustment(factor, playerZSpeed);

    if (static_cast<int8_t>(static_cast<uint8_t>(differenceAngle >> 8)) < 0)
        adjust = static_cast<int16_t>(-adjust);

    s.YAngle = WrapAngle(s.YAngle + static_cast<double>(adjust) * dtRatio);
}

// CalculateSteering — PhysicsFloatV2.cs:1405
// Sets YRotationAcceleration from player input, the section's own curvature,
// and how far the car's heading has drifted from the road's.
void CalculateSteering(PhysicsStateF& s, const FV2Track& t, double playerZSpeed,
                       int8_t leftRightValue, double dtRatio) {
    int roadSection = s.RoadSection;
    const FV2RoadSection* sec   = &t.Sections[roadSection];
    const FV2RoadPiece*   piece = &FV2_GetPiece(sec->PieceIndex);

    int16_t reversed = static_cast<int16_t>((sec->Angle & 0x10) ? -32768 : 0);
    int16_t diffAngle = static_cast<int16_t>(
        static_cast<int16_t>(static_cast<int16_t>(static_cast<int>(s.SectionYAngle)) -
                             static_cast<int16_t>(static_cast<int>(s.YAngle))) ^ reversed);

    uint8_t sectionByte = piece->SectionByte1;
    uint8_t curveBit = (piece->CurveDirection & 1) ? 128u : 0u;
    int16_t curveSign = static_cast<int16_t>(curveBit << 8);

    // Curved sections carry a fixed heading offset (217 units) either way.
    int bendIdx = 0;
    if (static_cast<int8_t>(sectionByte) < 0) {
        bendIdx = 1;
        if (static_cast<int16_t>(curveSign ^ reversed) < 0) bendIdx = 2;
    }
    static const int16_t kBend[3] = { 0, 217, -217 };
    diffAngle = static_cast<int16_t>(diffAngle + kBend[bendIdx]);

    int16_t absDiff = (diffAngle >= 0) ? diffAngle : static_cast<int16_t>(-diffAngle);
    int16_t signedDiff = diffAngle;
    int16_t scaledDiff = (static_cast<uint16_t>(absDiff) < 2048)
                       ? static_cast<int16_t>(absDiff << 4) : 32767;

    // Near the end of a section, steer for the *next* one.
    if (static_cast<uint8_t>(static_cast<uint8_t>(piece->CoordCount - 1) -
        static_cast<uint8_t>(static_cast<int16_t>(static_cast<int>(s.DistanceIntoSection)) >> 8)) < 2) {
        if (++roadSection >= t.SectionCount) roadSection = 0;
        sec   = &t.Sections[roadSection];
        piece = &FV2_GetPiece(sec->PieceIndex);
        reversed    = static_cast<int16_t>((sec->Angle & 0x10) ? -32768 : 0);
        sectionByte = piece->SectionByte1;
        curveBit    = (piece->CurveDirection & 1) ? 128u : 0u;
    }

    uint8_t reversedHi = static_cast<uint8_t>((reversed >> 8) & 0xFF);
    int8_t  curveDir = static_cast<int8_t>(static_cast<uint8_t>(curveBit ^ reversedHi));
    uint8_t steeringAmount = piece->SteeringAmount;

    int16_t yAccel = 0;
    int16_t zSpeedS = SaturateToShort(playerZSpeed);
    bool align;

    if (leftRightValue == 0) {
        if (static_cast<int8_t>(sectionByte) >= 0) {
            // Straight, no input: nothing to do but re-align.
            yAccel = 0;
            align = true;
        } else {
            // Curve, no input: the road steers for you.
            leftRightValue = curveDir;
            yAccel = ComputeSteeringAcceleration(steeringAmount, zSpeedS, leftRightValue);
            align = static_cast<uint8_t>(absDiff >> 8) >= 30;
        }
    } else {
        uint8_t agrees = static_cast<uint8_t>(static_cast<uint8_t>(leftRightValue) ^
                                              static_cast<uint8_t>(signedDiff >> 8));
        uint8_t amount;
        if (static_cast<int8_t>(sectionByte) >= 0) {
            amount = steeringAmount;
            if (static_cast<int8_t>(agrees) >= 0)
                amount = static_cast<uint8_t>(amount + static_cast<uint8_t>(scaledDiff >> 8));
        } else if (static_cast<int8_t>(static_cast<uint8_t>(
                       static_cast<uint8_t>(leftRightValue) ^ static_cast<uint8_t>(curveDir))) < 0) {
            // Steering against the bend: reduced authority.
            leftRightValue = curveDir;
            amount = static_cast<uint8_t>(steeringAmount - 35);
        } else {
            // Steering into the bend: extra authority.
            amount = static_cast<uint8_t>(steeringAmount + 45);
            if (static_cast<int8_t>(agrees) >= 0)
                amount = static_cast<uint8_t>(amount + static_cast<uint8_t>(scaledDiff >> 8));
        }
        yAccel = ComputeSteeringAcceleration(amount, zSpeedS, leftRightValue);
        align = static_cast<uint8_t>(absDiff >> 8) >= 30;
    }

    // Debug channel (see Physics_FloatV2.h). signedDiff is the heading error
    // the loop is chasing; align tells us whether the hard YAngle snap fired.
    gDbgHeadingErr = static_cast<double>(signedDiff);
    gDbgAlignFired = align ? 1 : 0;
    gDbgLeftRight  = static_cast<int>(leftRightValue);

    if (align) AlignCarWithRoad(s, absDiff, signedDiff, zSpeedS, dtRatio);

    yAccel = static_cast<int16_t>(yAccel - SaturateToShort(s.YRotationSpeed));
    if (s.TouchingRoad == 0) yAccel = 0;
    s.YRotationAcceleration = yAccel;
}

// --- Sub-steps 13-15: to world space, drag, rotation ----------------------

// CalculateWorldAcceleration — PhysicsFloatV2.cs:1574
TotalAccel CalculateWorldAcceleration(const ScArray& sc, const TotalAccel& l) {
    return TotalAccel{
        l.x * sc[22] + l.y * sc[20] + l.z * sc[2],
        l.x * sc[24] + l.y * sc[15] + l.z * sc[4],
        l.x * sc[23] + l.y * sc[21] + l.z * sc[3],
    };
}

// ReduceWorldAcceleration — PhysicsFloatV2.cs:1581
// Speed-proportional drag. The strength depends on what the car is doing:
// hard road contact, off-map, wrecked and chained cars all drag differently.
void ReduceWorldAcceleration(const PhysicsStateF& s, double carToRoadCollisionZAccel,
                             double xSpeed, double zSpeed, TotalAccel& t) {
    int shift = 1;
    bool freeRolling = false;
    double drag = 0.0;

    if (s.TouchingRoad != 0) {
        uint8_t impact = static_cast<uint8_t>(SaturateToShort(carToRoadCollisionZAccel) >> 8);
        int magnitude = (impact & 0x80) ? (impact ^ 0xFF) : impact;
        if (magnitude >= 3) {
            drag = 24576.0;                     // heavy landing
        } else if (static_cast<int8_t>(s.OffMapStatus) < 0) {
            drag = 24576.0;
        } else if (((s.WreckWheelHeightReduction >> 8) & 0xFF) != 0) {
            shift = 3;
            drag = 24576.0;                     // wrecked
        } else {
            freeRolling = true;
        }
    } else {
        freeRolling = true;
    }

    if (freeRolling) {
        if (s.CarOnChainsCountdown != 0) {
            shift = 3;
            drag = 24576.0;
        } else {
            double speed = std::max(std::fabs(xSpeed), std::fabs(zSpeed));
            shift = 5;
            // Slipstream: less drag when tucked in behind the opponent.
            if (static_cast<int8_t>(s.PlayerCloseToOpponent) < 0 &&
                static_cast<int8_t>(s.OpponentBehindPlayer) >= 0) {
                speed -= 2560.0;
                if (speed < 0.0) speed = 0.0;
            }
            drag = speed;
        }
    }

    const double scale = drag / 65536.0 / static_cast<double>(1 << shift);
    t.x -= scale * s.WorldXSpeed;
    t.y -= scale * s.WorldYSpeed;
    t.z -= scale * s.WorldZSpeed;
}

// CalculateXZRotationAcceleration — PhysicsFloatV2.cs:1645
// Suspension imbalance becomes pitch (X) and roll (Z) acceleration, damped by
// the current rotation speed.
struct RotAccel { double x, z; };
RotAccel CalculateXZRotationAcceleration(const PhysicsStateF& s, double overallBelow,
                                         double frontBelow, double localZAccel) {
    double pitch = overallBelow - s.XRotationSpeed / 16.0;
    if (s.TouchingRoad != 0) pitch += localZAccel / 4.0;   // squat under power
    return RotAccel{ pitch, frontBelow - s.ZRotationSpeed / 16.0 };
}

// UpdateRotationSpeeds — PhysicsFloatV2.cs:1659
void UpdateRotationSpeeds(PhysicsStateF& s, double rx, double ry, double rz, double dtRatio) {
    s.XRotationSpeed += ReduceValue(rx) * dtRatio;
    s.YRotationSpeed += ReduceValue(ry) * dtRatio;
    s.ZRotationSpeed += ReduceValue(rz) * dtRatio;
}

// CalculateFinalRotationSpeeds — PhysicsFloatV2.cs:1666
struct FinalRot { double x, y, z; };
FinalRot CalculateFinalRotationSpeeds(const PhysicsStateF& s, const ScArray& sc) {
    FinalRot f{};
    f.y = s.XRotationSpeed * sc[16] + s.YRotationSpeed * sc[17];
    f.x = s.XRotationSpeed * sc[17] + s.YRotationSpeed * sc[18];
    f.z = f.y * sc[4] + s.ZRotationSpeed;
    return f;
}

// --- Sub-steps 16-17: integration ----------------------------------------

// UpdateWorldSpeeds — PhysicsFloatV2.cs:1673
void UpdateWorldSpeeds(PhysicsStateF& s, const TotalAccel& t, double dtRatio) {
    s.WorldXSpeed += ReduceValue(t.x) * dtRatio;
    s.WorldYSpeed += ReduceValue(t.y) * dtRatio;
    s.WorldZSpeed += ReduceValue(t.z) * dtRatio;
}

// UpdatePosition — PhysicsFloatV2.cs:1680
// Integrate position and angles, then clamp pitch/roll. The limits tighten
// when the car is at the side of the road (AtSideByte == 224) so it can't
// lean absurdly far over a verge.
void UpdatePosition(PhysicsStateF& s, const FinalRot& f, double dtRatio) {
    s.WorldX += ReduceValue(s.WorldXSpeed) *  64.0 * dtRatio;
    s.WorldY += ReduceValue(s.WorldYSpeed) * 128.0 * dtRatio;
    s.WorldZ += ReduceValue(s.WorldZSpeed) *  64.0 * dtRatio;
    if (s.WorldY >= 65536000.0) s.WorldY = 65536000.0;

    s.XAngle += ReduceValue(f.x) * dtRatio;
    s.YAngle  = WrapAngle(s.YAngle + ReduceValue(f.y) * dtRatio);
    s.ZAngle += ReduceValue(f.z) * dtRatio;

    bool atSide = (static_cast<int8_t>(s.B1bb75) < 0) && (s.AtSideByte == 224);
    double maxAngle = atSide ?  2560.0 : 11264.0;
    double minAngle = atSide ? -2816.0 : -11520.0;

    if (s.XAngle > maxAngle) {
        s.XAngle = maxAngle;
        if (s.XRotationSpeed >= 0.0) s.XRotationSpeed = 0.0;
    } else if (s.XAngle < minAngle) {
        s.XAngle = minAngle;
        if (s.XRotationSpeed < 0.0) s.XRotationSpeed = 0.0;
    }

    if (s.ZAngle > maxAngle) {
        s.ZAngle = maxAngle;
        if (s.ZRotationSpeed >= 0.0) s.ZRotationSpeed = 0.0;
    } else if (s.ZAngle < minAngle) {
        s.ZAngle = minAngle;
        if (s.ZRotationSpeed < 0.0) s.ZRotationSpeed = 0.0;
    }
}

} // anonymous namespace

int    gFloatV2DebugSteps = 0;       // N key arms a burst; see Physics_FloatV2.h
double gDbgRoadFL = 0, gDbgRoadFR = 0, gDbgRoadR = 0;
double gDbgActFL  = 0, gDbgActFR  = 0, gDbgActR  = 0;
double gDbgBelowFL = 0, gDbgBelowFR = 0, gDbgBelowR = 0;
double gDbgZSpeed = 0, gDbgEngineIn = 0, gDbgEngineOut = 0, gDbgGrip = 0;
double gDbgCollY = 0, gDbgCollZ = 0, gDbgGravZ = 0, gDbgTotalZ = 0, gDbgWorldZSpeed = 0;
double gDbgYAngle = 0, gDbgSectionYAngle = 0, gDbgHeadingErr = 0;
double gDbgYRotSpeed = 0, gDbgYRotAccel = 0;
int    gDbgAlignFired = 0, gDbgAtSideByte = 0, gDbgLeftRight = 0;
double gDbgXAngle = 0, gDbgZAngle = 0, gDbgXRotSpeed = 0, gDbgZRotSpeed = 0;
double gDbgRawRoadFL = 0, gDbgPosZSpeed = 0, gDbgSurfZ = 0;
int    gDbgBlendUsed = 0;
int    gDbgFV2Section = 0, gDbgFV2Seg = 0, gDbgFV2ZFrac = 0, gDbgFV2XFrac = 0;

bool   gUseFloatV2Physics = true;        // now the default path; V toggles back to legacy
double gFloatV2Dt         = 1.0 / 60.0;  // 60Hz; B cycles 10 -> 25 -> 60
bool   gFloatV2NeedsSeed  = true;    // set whenever the legacy path has run
bool   gUseFloatV2Opponent        = true;   // O toggles back to the 8.3Hz opponent
bool   gFloatV2OpponentNeedsSeed  = true;   // set whenever the legacy opponent has run
bool   gFloatV2UnreverseCurveDist = true;    // ON by default; J toggles. See header.
bool   gFloatV2DumpOnCurves       = false;   // K toggles; see header

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
//   4. CalculateRoadWheelHeights   (via the Track_FloatV2 adapter)
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
    const FV2Track* track = FV2_GetTrack();
    WheelRoadH roadH{};
    if (track) roadH = CalculateRoadWheelHeights(state, *track, wheels, dtRatio);
    WheelActualH actualH = CalculateActualWheelHeights(state, sc);
    LocalSpeed   local   = CalculateXZSpeeds(state, sc);
    state.PlayersZSpeed  = local.z;
    state.PlayersXSpeed  = local.x;
    SetWheelRotationSpeed(state, local.z, dtRatio);
    GravityXYZ grav = CalculateGravityAcceleration(sc);
    // Steering input, or the chain-drag value if the car is being towed.
    int8_t leftRightValue = 0;
    if (state.TouchingRoad != 0) {
        if (state.CarOnChainsCountdown != 0)
            leftRightValue = static_cast<int8_t>(state.CarOnChainsCountdown);
        else if (input.Left && !input.Right) leftRightValue = -15;
        else if (input.Right)                leftRightValue =  15;
    }

    CollisionResult coll = CarCollisionDetection(state, roadH, actualH, dtRatio);

    TotalAccel total{};
    FinalRot   finalRot{};

    // Off the map entirely: no forces at all, the car just coasts and falls.
    if (state.CarOnTrack != 0) {
        CollAccel collAccel = CalculateCarCollisionAcceleration(state, coll);
        double carToRoadCollisionZAccel = collAccel.z;

        // Car-to-car impulses were accumulated in 10Hz units by the collision
        // code, so divide by dtRatio to spread them over the faster steps.
        collAccel.x += static_cast<double>(state.CarToCarXAcceleration) / dtRatio;
        collAccel.y += static_cast<double>(state.CarToCarYAcceleration) / dtRatio;
        collAccel.z += static_cast<double>(state.CarToCarZAcceleration) / dtRatio;
        state.CarToCarXAcceleration = 0;
        state.CarToCarYAcceleration = 0;
        state.CarToCarZAcceleration = 0;

        total = CalculateTotalAcceleration(state, grav, collAccel, local.x, local.z);

        if (track) CalculateSteering(state, *track, local.z, leftRightValue, dtRatio);
        double yRotationAcceleration = state.YRotationAcceleration;
        double localZAcceleration    = total.z;

        total = CalculateWorldAcceleration(sc, total);
        ReduceWorldAcceleration(state, carToRoadCollisionZAccel, local.x, local.z, total);

        RotAccel rot = CalculateXZRotationAcceleration(state, coll.overallBelow,
                                                       coll.frontBelow, localZAcceleration);
        UpdateRotationSpeeds(state, rot.x, yRotationAcceleration, rot.z, dtRatio);
        finalRot = CalculateFinalRotationSpeeds(state, sc);
    }

    UpdateWorldSpeeds(state, total, dtRatio);
    UpdatePosition(state, finalRot, dtRatio);

    gDbgRoadFL = roadH.fl;   gDbgRoadFR = roadH.fr;   gDbgRoadR = roadH.r;
    gDbgActFL  = actualH.fl; gDbgActFR  = actualH.fr; gDbgActR  = actualH.r;
    gDbgBelowFL = coll.flBelow; gDbgBelowFR = coll.frBelow; gDbgBelowR = coll.rBelow;
    gDbgTotalZ = total.z; gDbgWorldZSpeed = state.WorldZSpeed;
    gDbgYAngle        = state.YAngle;
    gDbgSectionYAngle = state.SectionYAngle;
    gDbgYRotSpeed     = state.YRotationSpeed;
    gDbgYRotAccel     = state.YRotationAcceleration;
    gDbgAtSideByte    = static_cast<int>(state.AtSideByte);
    gDbgXAngle = state.XAngle; gDbgZAngle = state.ZAngle;
    gDbgXRotSpeed = state.XRotationSpeed; gDbgZRotSpeed = state.ZRotationSpeed;
}

// --- Persistent state / step entry point -----------------------------------
// CopyLegacyToFloatV2 / CopyFloatV2ToLegacy live in Car_Behaviour.cpp (see
// header) because the legacy globals they touch are file-static there.

PhysicsStateF& FloatV2_State()
{
    static PhysicsStateF state{};
    return state;
}

void FloatV2_RunStep(const PhysicsInput& input, double dt)
{
    PhysicsStateF& s = FloatV2_State();

    if (gFloatV2NeedsSeed) {
        // First step after the toggle flips (or after the car was repositioned):
        // take the legacy car state wholesale so the swap is seamless mid-drive.
        CopyLegacyToFloatV2(s);
        gFloatV2NeedsSeed = false;
    } else {
        CopyLegacyRoadStateToFloatV2(s);
    }

    PhysicsStepF_Tick(s, input, dt);
    CopyFloatV2ToLegacy(s);
}


} // namespace scr
