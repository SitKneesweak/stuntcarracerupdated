// Sim_Trace.cpp — see Sim_Trace.h for what this is for.

// dxstdafx.h first: Car_Behaviour.h's prototypes use DWORD, which on the
// portable builds comes from dx_linux.h via here. (Never reach for windows.h to
// satisfy it -- dx_linux.h typedefs the same names to different underlying
// types and the two cannot coexist.)
#include "dxstdafx.h"

#include "Sim_Trace.h"
#include "Physics_FloatV2.h"
#include "Det_Rand.h"
#include "Car_Behaviour.h"      // KEY_P1_*

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace scr {

bool     gSimTraceEnabled  = false;
bool     gSimTraceVerbose  = false;
long     gSimTraceMaxSteps = 6000;      // 100 seconds at 60Hz
int      gSimTraceTrack    = 0;
uint32_t gSimTraceSeed     = 0x12345678u;

static char  sLogPath[512] = "simtrace.log";
static FILE* sLog          = nullptr;
static long  sStep         = 0;

/*	=====================================================================================
	Hashing

	FNV-1a, 64-bit. Chosen for being three lines of code with no tables and no
	endian- or width-dependent tricks, which matters more here than avalanche
	quality: any single-bit change anywhere in the state has to change the hash,
	and FNV-1a does that.

	Everything is fed as a uint64_t of the value's *bit pattern*. Doubles go
	through memcpy rather than a union or a pointer cast, so there is no strict
	aliasing question and no chance of the compiler reading it as anything else.

	The struct is emphatically NOT hashed as a block of memory. PhysicsStateF
	interleaves doubles with uint8_t and bool, so it is full of padding bytes
	that no one ever writes; hashing those would compare uninitialised memory and
	produce spurious divergence. Every field is listed out instead.
	===================================================================================== */

static const uint64_t FNV_OFFSET = 0xcbf29ce484222325ull;
static const uint64_t FNV_PRIME  = 0x100000001b3ull;

static inline void HashBytes(uint64_t& h, uint64_t v)
{
	for (int i = 0; i < 8; ++i)
		{
		h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xffu);
		h *= FNV_PRIME;
		}
}

static inline uint64_t BitsOf(double d)
{
	uint64_t bits;
	memcpy(&bits, &d, sizeof(bits));
	// -0.0 and +0.0 compare equal but have different bit patterns, and the two
	// can legitimately arise from different-but-equivalent orderings. Normalise
	// so a sign bit on zero is not reported as a divergence. NaN is left alone:
	// a NaN appearing on one platform only is exactly the sort of thing this is
	// meant to catch.
	if (d == 0.0) bits = 0;
	return bits;
}

// One accumulator that both hashes and (optionally) prints, so the verbose dump
// can never drift out of step with what is hashed.
struct Field
{
	uint64_t h;
	FILE*    dump;      // null unless verbose

	void operator()(const char* name, double v)
		{
		HashBytes(h, BitsOf(v));
		if (dump) fprintf(dump, " %s=%016llx", name,
						  static_cast<unsigned long long>(BitsOf(v)));
		}
	void operator()(const char* name, int64_t v)
		{
		HashBytes(h, static_cast<uint64_t>(v));
		if (dump) fprintf(dump, " %s=%lld", name, static_cast<long long>(v));
		}
	void operator()(const char* name, uint8_t v) { (*this)(name, static_cast<int64_t>(v)); }
	void operator()(const char* name, int16_t v) { (*this)(name, static_cast<int64_t>(v)); }
	void operator()(const char* name, int32_t v) { (*this)(name, static_cast<int64_t>(v)); }
	void operator()(const char* name, bool v)    { (*this)(name, static_cast<int64_t>(v ? 1 : 0)); }
};

// Every field of PhysicsStateF, in declaration order. Keep this in step with the
// struct: a field left out here is a field that can diverge silently.
#define SIMTRACE_FIELDS(F, s)                                                 \
	F("WorldX", s.WorldX) F("WorldY", s.WorldY) F("WorldZ", s.WorldZ)         \
	F("XAng", s.XAngle) F("YAng", s.YAngle) F("ZAng", s.ZAngle)               \
	F("WXSpd", s.WorldXSpeed) F("WYSpd", s.WorldYSpeed) F("WZSpd", s.WorldZSpeed) \
	F("XRSpd", s.XRotationSpeed) F("YRSpd", s.YRotationSpeed) F("ZRSpd", s.ZRotationSpeed) \
	F("FLDmg", s.FrontLeftDamage) F("FRDmg", s.FrontRightDamage) F("RDmg", s.RearDamage) \
	F("FLDmgR", s.FrontLeftDamageRemainder) F("FRDmgR", s.FrontRightDamageRemainder)      \
	F("RDmgR", s.RearDamageRemainder)                                         \
	F("OldFLD", s.OldFrontLeftDiff) F("OldFRD", s.OldFrontRightDiff) F("OldRD", s.OldRearDiff) \
	F("Sect", s.RoadSection) F("Dist", s.DistanceIntoSection) F("Lap", s.Lap) \
	F("EngPwr", s.EnginePower) F("BoostUnit", s.BoostUnit)                    \
	F("BoostMax", s.BoostMaxUnits) F("BoostAct", s.BoostActivated)            \
	F("BoostRes", s.BoostReserve) F("BoostUV", s.BoostUnitValue)              \
	F("Accel", s.Accelerating) F("WreckWHR", s.WreckWheelHeightReduction)     \
	F("Smashed", s.SmashedCountdown) F("Touching", s.TouchingRoad)            \
	F("SectYAng", s.SectionYAngle) F("WheelRot", s.WheelRotationSpeed)        \
	F("Cushion", s.RoadCushionValue) F("OffMap", s.OffMapStatus)              \
	F("AtSide", s.AtSideByte) F("WhichSide", s.WhichSideByte) F("B1bb75", s.B1bb75) \
	F("RoadX", s.PlayersRoadXPosition) F("NormDist", s.NormalDistanceIntoSection) \
	F("PZSpd", s.PlayersZSpeed) F("PosPZSpd", s.PosPlayersZSpeed) F("PXSpd", s.PlayersXSpeed) \
	F("FLRoad", s.FrontLeftRoadHeight) F("FRRoad", s.FrontRightRoadHeight) F("RRoad", s.RearRoadHeight) \
	F("FLBelow", s.FrontLeftAmountBelowRoad) F("FRBelow", s.FrontRightAmountBelowRoad)      \
	F("RBelow", s.RearAmountBelowRoad)                                        \
	F("FLGnd", s.FrontLeftGrounded) F("FRGnd", s.FrontRightGrounded) F("RGnd", s.RearGrounded) \
	F("DmgCount", s.DamagedCount) F("DmgLimit", s.DamagedLimit) F("Damaged", s.Damaged)  \
	F("DmgValue", s.DamageValue)                                              \
	F("GndCount", s.GroundedCount) F("Fourteen", s.FourteenFramesElapsed)     \
	F("Chains", s.CarOnChainsCountdown)                                       \
	F("RoadID", s.RoadID) F("OnTrack", s.CarOnTrack) F("Super", s.IsSuperLeague) \
	F("FLHDiff", s.FrontLeftHeightDifference) F("FRHDiff", s.FrontRightHeightDifference)  \
	F("RHDiff", s.RearHeightDifference)                                       \
	F("EngZAcc", s.EngineZAcceleration) F("CollAir", s.CollisionInAir)        \
	F("YRotAcc", s.YRotationAcceleration) F("RearSurfX", s.RearWheelSurfaceXPosition) \
	F("C2CX", s.CarToCarXAcceleration) F("C2CY", s.CarToCarYAcceleration)     \
	F("C2CZ", s.CarToCarZAcceleration)                                        \
	F("PClose", s.PlayerCloseToOpponent) F("OppBehind", s.OpponentBehindPlayer)

/*	=====================================================================================
	Scripted input

	Depends on the step counter and nothing else, so both platforms drive the
	identical car. Accelerate is held down throughout and the steering is
	re-rolled every SEGMENT steps from a small LCG over the segment index --
	authored steering would only ever visit the corners I thought to write down,
	whereas this wanders across the track, hits the barriers, crashes and gets
	craned back on, which is the state space worth checking.

	The LCG here is a *separate* stream from Det_Rand and is not the simulation
	RNG: it stands in for the player's hands, so it must not be perturbed by
	anything the simulation does.
	===================================================================================== */

uint32_t SimTrace_ScriptedInput()
{
	const long SEGMENT = 24;                    // ~0.4s at 60Hz
	uint32_t seg = static_cast<uint32_t>(sStep / SEGMENT);

	// One round of a Knuth LCG, then take the high bits (the low bits of an LCG
	// have short periods).
	uint32_t r = seg * 1664525u + 1013904223u;
	r ^= r >> 16;
	r = r * 2246822519u;
	r ^= r >> 13;

	uint32_t in = KEY_P1_ACCEL;

	// Steering: 2 of every 8 segments left, 2 right, 4 straight ahead. Biased
	// toward straight on purpose -- an even split wanders off the track within a
	// couple of sections and spends the run being craned back on, which covers
	// the recovery path well but never reaches a section crossing or a lap.
	switch (r & 7)
		{
		case 0: case 1: in |= KEY_P1_LEFT;  break;
		case 2: case 3: in |= KEY_P1_RIGHT; break;
		default: break;
		}

	// Boost on 1 segment in 16, brake on 1 in 32 -- both rare, but they have to
	// appear or the boost/damage counters never move.
	if (((r >> 3) & 15) == 0) in |= KEY_P1_BOOST;
	if (((r >> 7) & 31) == 0) in |= KEY_P1_BRAKE;

	return in;
}

/*	===================================================================================== */

int SimTrace_ParseArg(int argc, char** argv, int i)
{
	const bool plain   = !strcmp(argv[i], "--simtrace");
	const bool verbose = !strcmp(argv[i], "--simtrace-verbose");
	if (plain || verbose)
		{
		gSimTraceEnabled = true;
		if (verbose) gSimTraceVerbose = true;
		// Optional step count, but only if the next argument is a number --
		// otherwise "--simtrace --fullscreen" would eat the wrong thing.
		if ((i + 1 < argc) && (argv[i+1][0] >= '0') && (argv[i+1][0] <= '9'))
			{
			gSimTraceMaxSteps = atol(argv[i+1]);
			return 2;
			}
		return 1;
		}
	if (!strcmp(argv[i], "--simtrace-track") && (i + 1 < argc))
		{
		gSimTraceTrack = atoi(argv[i+1]);
		return 2;
		}
	if (!strcmp(argv[i], "--simtrace-seed") && (i + 1 < argc))
		{
		gSimTraceSeed = static_cast<uint32_t>(strtoul(argv[i+1], nullptr, 0));
		return 2;
		}
	if (!strcmp(argv[i], "--simtrace-out") && (i + 1 < argc))
		{
		snprintf(sLogPath, sizeof(sLogPath), "%s", argv[i+1]);
		return 2;
		}
	return 0;
}

void SimTrace_Begin()
{
	if (!gSimTraceEnabled || sLog) return;

	// main() has already chdir'd to SDL_GetBasePath(), so a bare filename lands
	// next to the executable -- which is the point: the Windows build is run on
	// a machine with no shared terminal, and the log has to be copyable back.
	sLog = fopen(sLogPath, "w");
	if (!sLog)
		{
		printf("simtrace: cannot open %s for writing, tracing disabled\n", sLogPath);
		gSimTraceEnabled = false;
		return;
		}

	sStep = 0;
	det::SeedRand(gSimTraceSeed);

	// The header records everything that would change the numbers below it, so a
	// mismatched pair of logs is obvious at the first line rather than after an
	// hour of bisecting. dt is printed as its bit pattern for the same reason
	// the state is: 1.0/60.0 has to be the same double on both sides.
	uint64_t dtBits = BitsOf(gFloatV2Dt);
	fprintf(sLog, "# stuntcarracer simtrace v1\n");
	fprintf(sLog, "# track=%d seed=0x%08lx steps=%ld dt=%.17g dtbits=%016llx\n",
			gSimTraceTrack, static_cast<unsigned long>(gSimTraceSeed),
			gSimTraceMaxSteps, gFloatV2Dt,
			static_cast<unsigned long long>(dtBits));
	fprintf(sLog, "# verbose=%d\n", gSimTraceVerbose ? 1 : 0);
	fflush(sLog);

	printf("simtrace: recording %ld steps to %s (track %d, seed 0x%08lx)\n",
		   gSimTraceMaxSteps, sLogPath, gSimTraceTrack,
		   static_cast<unsigned long>(gSimTraceSeed));
}

void SimTrace_RecordStep(const PhysicsStateF& s)
{
	if (!sLog || sStep >= gSimTraceMaxSteps) return;

	Field f{ FNV_OFFSET, nullptr };

	if (gSimTraceVerbose)
		{
		fprintf(sLog, "%08ld", sStep);
		f.dump = sLog;
		}

	#define SIMTRACE_EMIT(name, value) f(name, value);
	SIMTRACE_FIELDS(SIMTRACE_EMIT, s)
	#undef SIMTRACE_EMIT

	if (gSimTraceVerbose)
		fprintf(sLog, " hash=%016llx\n", static_cast<unsigned long long>(f.h));
	else
		fprintf(sLog, "%08ld %016llx\n", sStep,
				static_cast<unsigned long long>(f.h));

	++sStep;

	// Flushed every step on purpose. The runs end in a crash often enough (and
	// are killed by hand often enough) that a buffered tail would be the part
	// worth reading.
	fflush(sLog);
}

bool SimTrace_Finished()
{
	return gSimTraceEnabled && (sStep >= gSimTraceMaxSteps);
}

void SimTrace_End()
{
	if (!sLog) return;
	fprintf(sLog, "# end %ld steps\n", sStep);
	fclose(sLog);
	sLog = nullptr;
	printf("simtrace: wrote %ld steps to %s\n", sStep, sLogPath);
}

} // namespace scr
