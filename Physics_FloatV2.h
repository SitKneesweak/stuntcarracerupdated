// Physics_FloatV2 — C++ port of StuntCarRacer.PhysicsFloatV2 (from stuntcarracer.net .NET WASM build).
//
// The .NET version uses doubles + a timestep-parameterised tick so the same
// physics runs cleanly at 10Hz (Amiga rate) or higher (60Hz). Behaviour derives
// from the original 68k assembly via an intermediate 1:1 float port.
//
// Reference source (decompiled, gitignored): reference/floatv2-decompiled/PhysicsFloatV2.cs
//
// This port is a WORK IN PROGRESS. When enabled at runtime, it replaces the
// legacy CarBehaviour(). While disabled, the legacy fixed-point path continues
// to run unchanged.

#pragma once

#include <cstdint>

namespace scr {

// Mirrors StuntCarRacer.PhysicsFloatV2.PhysicsStateF (see reference for docs).
// Field names kept close to the C# for easier cross-referencing during the port.
struct PhysicsStateF
{
    double WorldX, WorldY, WorldZ;
    double XAngle, YAngle, ZAngle;
    double WorldXSpeed, WorldYSpeed, WorldZSpeed;
    double XRotationSpeed, YRotationSpeed, ZRotationSpeed;

    uint8_t FrontLeftDamage, FrontRightDamage, RearDamage;
    double  FrontLeftDamageRemainder, FrontRightDamageRemainder, RearDamageRemainder;
    double  OldFrontLeftDiff, OldFrontRightDiff, OldRearDiff;

    uint8_t RoadSection;
    double  DistanceIntoSection;
    uint8_t Lap;
    int16_t EnginePower;
    double  BoostUnit;
    uint8_t BoostMaxUnits, BoostActivated, BoostReserve, BoostUnitValue;
    uint8_t Accelerating;
    int32_t WreckWheelHeightReduction;
    uint8_t SmashedCountdown, TouchingRoad;
    double  SectionYAngle;
    double  WheelRotationSpeed;
    uint8_t RoadCushionValue, OffMapStatus;
    uint8_t AtSideByte, WhichSideByte, B1bb75;

    double  PlayersRoadXPosition;
    double  NormalDistanceIntoSection;
    double  PlayersZSpeed, PosPlayersZSpeed;
    // Not in the C# PhysicsStateF: CalculateXZSpeeds' lateral output is a
    // local there. The legacy code keeps it in player_x_speed (LimitViewpointY
    // and the skid sound read it), so Tick stores it for the adapter.
    double  PlayersXSpeed;

    double  FrontLeftRoadHeight,  FrontRightRoadHeight,  RearRoadHeight;
    double  FrontLeftAmountBelowRoad, FrontRightAmountBelowRoad, RearAmountBelowRoad;

    // Per-wheel "currently in contact" latch. The Amiga tests the previous
    // step's penetration (>= 0x400 now, < 0x200 then), which only fires if the
    // wheel crosses the whole band in a single step - true at its 10Hz rate,
    // false at 60Hz where the wheel eases through the band and no step sees
    // both conditions. The latch is the same edge detector expressed in a
    // rate-independent way. See ProcessWheel.
    bool    FrontLeftGrounded, FrontRightGrounded, RearGrounded;

    uint8_t DamagedCount, DamagedLimit, Damaged;
    int16_t DamageValue;
    uint8_t GroundedCount, FourteenFramesElapsed, CarOnChainsCountdown;
    uint8_t RoadID, CarOnTrack;
    bool    IsSuperLeague;

    double  FrontLeftHeightDifference, FrontRightHeightDifference, RearHeightDifference;
    double  EngineZAcceleration;
    uint8_t CollisionInAir;
    double  YRotationAcceleration;
    uint8_t RearWheelSurfaceXPosition;
    int16_t CarToCarXAcceleration, CarToCarYAcceleration, CarToCarZAcceleration;
    uint8_t PlayerCloseToOpponent, OpponentBehindPlayer;
};

struct PhysicsInput
{
    bool Left, Right, Accelerate, Brake, Boost;
};

// One physics tick. dt defaults to 0.1s = 10Hz (Amiga rate, ground-truth
// behaviour). Reducing dt while keeping tuning identical is FloatV2's whole
// point — the internal ratio dt/0.1 propagates through every sub-step.
void PhysicsStepF_Tick(PhysicsStateF& state, const PhysicsInput& input, double dt = 0.1);

// Feature toggle. When false, the legacy CarBehaviour() runs unchanged.
extern bool gUseFloatV2Physics;

// Timestep handed to Tick. Defaults to 1/60. Press B to cycle 10 -> 25 -> 60Hz;
// 0.1 (10Hz) is the original Amiga rate and the reference for A/B'ing the port
// against the legacy path. The whole point of FloatV2 is that lowering this
// changes smoothness without changing how the car behaves.
extern double gFloatV2Dt;

// Feature toggle for the opponent (OpponentStepF in PhysicsFloatV2.cs). Only
// has any effect while gUseFloatV2Physics is on, because it steps the opponent
// on the player's clock. When false the opponent keeps running once per
// frameGap tick (~8.3Hz) exactly as the Amiga did. Press O to toggle.
extern bool gUseFloatV2Opponent;

// Set while the legacy opponent path has run, so the next FloatV2 opponent step
// re-seeds its doubles from the legacy opponent globals.
extern bool gFloatV2OpponentNeedsSeed;

// Set while the legacy path is driving, so the next FloatV2 step re-seeds from
// the legacy globals instead of continuing from stale state.
extern bool gFloatV2NeedsSeed;

// EXPERIMENT (J key) — recover NormalDistanceIntoSection from the legacy
// players_distance_into_section. Physics.cs:3672 keeps two distinct values:
// NormalDistanceIntoSection (raw, the piece's own order) and
// DistanceIntoSection = DetailNearRoad(Normal, numSegments, plus180), which is
// the plus180 mirror (Physics.cs:4055). The road-height lookup uses the Normal
// one and applies plus180 itself. But Track.cpp:1422 already builds
// Track[].coords in travel order for plus180 sections, so the legacy value we
// receive is the mirrored form. This un-mirrors it. See the fuller note on
// FV2_NormalDistanceIntoSection in Car_Behaviour.cpp.
extern bool gFloatV2UnreverseCurveDist;

// Dump every step spent on a curved piece (K key). Saves having to time the
// N key against a corner.
extern bool gFloatV2DumpOnCurves;

// --- Diagnostics -----------------------------------------------------------
// Counts down while non-zero; each FloatV2 step dumps its intermediate values
// to stdout and decrements it. Set by the 'N' key so a burst can be captured
// mid-drive without flooding the terminal. Tick fills the gDbg* values in;
// the hook in Car_Behaviour.cpp prints them next to the legacy equivalents,
// which is the only place both sets of units are visible at once.
extern int    gFloatV2DebugSteps;
extern double gDbgRoadFL, gDbgRoadFR, gDbgRoadR;
extern double gDbgActFL,  gDbgActFR,  gDbgActR;
extern double gDbgBelowFL, gDbgBelowFR, gDbgBelowR;
// The forward-force chain, in the order Tick builds it. A car that drives
// backwards has a sign flip somewhere along here.
extern double gDbgZSpeed, gDbgEngineIn, gDbgEngineOut, gDbgGrip;
extern double gDbgCollY, gDbgCollZ, gDbgGravZ, gDbgTotalZ, gDbgWorldZSpeed;
// Yaw / steering channel: separates "steering lag lets the car run wide" from
// "suspension spikes and throws it". gDbgHeadingErr is CalculateSteering's
// diffAngle (road heading minus car heading, plus the bend offset), which is
// the error the whole steering loop is working against.
extern double gDbgYAngle, gDbgSectionYAngle, gDbgHeadingErr;
extern double gDbgYRotSpeed, gDbgYRotAccel;
extern int    gDbgAlignFired, gDbgAtSideByte, gDbgLeftRight;
// Pitch/roll: the "is it flipping" signal, and the ±11264 / ±2560 clamps.
extern double gDbgXAngle, gDbgZAngle, gDbgXRotSpeed, gDbgZRotSpeed;
// Road-height lookup internals for the front-left wheel: the raw table result
// before the cushion blend, the blend fraction actually applied, and the
// PosPlayersZSpeed that decides whether the blend is bypassed at all.
extern double gDbgRawRoadFL, gDbgPosZSpeed, gDbgSurfZ;
extern int    gDbgBlendUsed;

// Adapters between legacy fixed-point globals (player_x, etc. in
// Car_Behaviour.cpp) and PhysicsStateF. Implemented in Car_Behaviour.cpp,
// where those file-static globals are visible.
//
// Unit mapping (derived from UpdatePlayersPosition/UpdatePlayersWorldSpeed
// in Car_Behaviour.cpp, and corroborated by the PLAY_AMIGA_RECORDING block
// which scales recorded Amiga longs by the same factors):
//   player_x, player_z  ==  WorldX, WorldZ  *  (PC_FACTOR * 4)  == * 8
//   player_y            ==  WorldY                             == * 1
//   angles, speeds, accelerations             1:1 (both use 65536 == 360 deg)
// Note player_y is *not* PC-scaled, and the render-side negation of X/Z
// angles happens later, at CarBehaviour's output — not here.
// Full seed — call when enabling the toggle or after the car is repositioned.
void CopyLegacyToFloatV2(PhysicsStateF& out);
// Per-step — only what the legacy code still owns (road tracking, car-to-car
// impulses, league state). Deliberately does not touch FloatV2's own state.
void CopyLegacyRoadStateToFloatV2(PhysicsStateF& out);
void CopyFloatV2ToLegacy(const PhysicsStateF& in);

// Runs one FloatV2 step against the legacy globals: copy in, tick, copy out.
// The caller must already have run the legacy road-position tracking for this
// frame (CalculateRoadWheelHeights), because Tick treats RoadSection /
// DistanceIntoSection / PlayersRoadXPosition / SectionYAngle as *inputs* —
// the .NET build maintains them outside the physics assembly too.
void FloatV2_RunStep(const PhysicsInput& input, double dt);

// Persistent state between steps (FloatV2 is authoritative for dynamics while
// the toggle is on; the legacy globals mirror it for the renderer and HUD).
PhysicsStateF& FloatV2_State();

} // namespace scr
