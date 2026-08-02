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

    double  FrontLeftRoadHeight,  FrontRightRoadHeight,  RearRoadHeight;
    double  FrontLeftAmountBelowRoad, FrontRightAmountBelowRoad, RearAmountBelowRoad;

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

// Adapters between legacy fixed-point globals (player_x, etc. in
// Car_Behaviour.cpp) and PhysicsStateF. Called once per tick on the boundary.
void CopyLegacyToFloatV2(PhysicsStateF& out);
void CopyFloatV2ToLegacy(const PhysicsStateF& in);

} // namespace scr
