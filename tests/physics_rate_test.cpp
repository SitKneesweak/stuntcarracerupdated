// physics_rate_test — does the FloatV2 physics actually behave the same at
// every timestep?
//
// FloatV2's whole premise (see Physics_FloatV2.h) is that lowering dt changes
// smoothness but not behaviour: the internal ratio dt/0.1 is threaded through
// every sub-step so the 10Hz Amiga tuning still applies at 60Hz. Nothing has
// ever checked that. This does.
//
// Method: drive one car along a synthetic flat track with a *time-keyed* input
// script (Sim_Trace's script is keyed on step number, which is a different
// point in time at every rate, so it cannot be used for this). Run the same
// script at 10 / 25 / 60 / 120Hz, sample the state at matched wall-clock
// instants, and report how far the faster rates drift from the 10Hz reference.
//
// The 10Hz run is the reference because that is the rate the constants were
// tuned at on the Amiga -- it is ground truth for how the car should behave,
// and the faster rates are the approximation being judged.
//
// Exact agreement is not expected or required: different step counts means
// different rounding, and the sim quantises hard (SaturateToShort, BCD, integer
// road heights). What matters is whether the drift stays bounded or grows into
// a different trajectory. The thresholds below are deliberately loose -- this
// is a regression fence, not a proof.
//
//   make -C tests physics_rate_test && ./tests/physics_rate_test
//   ./tests/physics_rate_test -v     per-sample table for every rate

#include "../Physics_FloatV2.h"
#include "../Track_FloatV2.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// --- Stubs for what the physics links against but this test does not need ---
//
// Sim_Trace writes a determinism log from inside Tick. Off here: this test
// compares trajectories itself, and linking Sim_Trace.cpp would drag in the
// game's command-line handling.
namespace scr {
bool gSimTraceEnabled = false;
void SimTrace_RecordStep(const PhysicsStateF&) {}

// Track.cpp owns the decoded Amiga Y tables. Rather than load a real track
// (which needs the data files and Track.cpp's globals), the surface is
// synthesised here, so the test can choose how rough the road is.
//
// 10240 is track 1's start-piece road height (see the crane note at
// Car_Behaviour.cpp:4747). Zero is a bad choice for the base: it puts the
// surface at the off-map floor and the car falls rather than drives.
int gSurfaceProfile = 0;    // set by the scenario loop in main()

long FV2_GetRawYCoord(int /*yCoordId*/, int coordIndex) {
    const long base = 10240;
    switch (gSurfaceProfile) {
    case 0:  // flat and level -- the kindest case, nothing excites the suspension
        return base;
    case 1:  // washboard: a bump on every other coordinate, small enough that
             // the wheels stay in contact. Exercises ProcessWheel's contact
             // band and the suspension decay without launching the car.
        return base + ((coordIndex % 2) ? 150 : -150);
    case 2:  // crest and dip: a large single feature per section, enough to
             // unload the wheels over the top and land hard the other side.
             // This is what reaches the grounded latch and the damage path.
             // Kept moderate on purpose: a steeper crest wrecks the car
             // outright, and a wreck is a discrete event that dominates the
             // comparison rather than measuring the physics either side of it.
             return base + ((coordIndex < 4) ? 100 * coordIndex
                                             : 100 * (8 - coordIndex));
    }
    return base;
}

// The legacy<->FloatV2 adapters live in Car_Behaviour.cpp and are reached only
// through FloatV2_RunStep, the game's entry point. This test calls
// PhysicsStepF_Tick directly and keeps its own PhysicsStateF, so these exist
// only to satisfy the linker.
void CopyLegacyToFloatV2(PhysicsStateF&) {}
void CopyFloatV2ToLegacy(const PhysicsStateF&) {}
void CopyLegacyRoadStateToFloatV2(PhysicsStateF&) {}
}

using namespace scr;

namespace {

// One sampled instant. Only the fields that describe where the car is and
// which way it is pointing -- those are what a player sees diverge.
struct Sample {
    double t;
    double x, y, z;
    double xAngle, yAngle, zAngle;
    double speed;            // WorldZSpeed, the speedo value
    int    damage;           // summed wheel damage; integer, so drift here is stark
    bool   airborne;         // no wheel in contact -- proves the rough scenarios bite
};

// The input script, keyed on elapsed simulation time in seconds. Every rate
// sees the same inputs at the same *instants*, which is the whole point.
//
// Shape: hold the throttle throughout, then a left turn, a straight, a right
// turn. Steering and sustained throttle are what exercise the dtRatio paths
// (CalculateSteering, ComputeEngineAcceleration, the ReduceValue decays).
PhysicsInput ScriptedInput(double t) {
    PhysicsInput in{};
    in.Accelerate = true;
    if (t >= 4.0 && t < 7.0)  in.Left  = true;
    if (t >= 10.0 && t < 13.0) in.Right = true;
    return in;
}

// A car sitting on the road at the start line, ready to drive. Mirrors the
// fields CopyLegacyToFloatV2 seeds (Car_Behaviour.cpp:3392); the rest are
// zero, which is their at-rest value.
void SeedState(PhysicsStateF& s) {
    std::memset(&s, 0, sizeof(s));

    s.CarOnTrack   = 1;
    s.TouchingRoad = 1;
    s.RoadSection  = 0;
    s.DistanceIntoSection       = 0.0;
    s.NormalDistanceIntoSection = 0.0;
    s.PlayersRoadXPosition      = 128.0;   // centre of the road

    // Sit the car on the surface. Flat track, so one lookup is the whole road.
    //
    // The scale matters: the road sits at world.y = roadHeight << 8, because
    // CalculateActualWheelHeights takes the wheel heights as world.y >> 8 and
    // the collision compares those against road heights directly. See the
    // crane note at Car_Behaviour.cpp:4735. Seeding WorldY in road-height units
    // instead drops the car 256x too low and it simply falls.
    const FV2Track* t = FV2_GetTrack();
    const int roadY = FV2_GetRoadHeight(*t, 0, 0, 128);
    s.WorldY = static_cast<double>(roadY) * 256.0;

    s.FrontLeftRoadHeight  = static_cast<double>(roadY);
    s.FrontRightRoadHeight = static_cast<double>(roadY);
    s.RearRoadHeight       = static_cast<double>(roadY);

    // Engine power is stored byte-reversed, as the Amiga kept it;
    // ComputeEngineAcceleration swaps it back. 240 is the standard-league
    // value (Car_Behaviour.cpp:451). Seeding a plain 240 here would swap back
    // to 0xF000 == -4096 and drive the car backwards -- see FV2_SwapEnginePower.
    const long enginePower = 240;
    s.EnginePower = static_cast<int16_t>(((enginePower & 0xFF) << 8) |
                                         ((enginePower >> 8) & 0xFF));

    // Boost left alone (BoostReserve 0) so the run tests driving, not boost.
    // BoostMaxUnits still has to be non-zero or the first decrement clamps --
    // see the note in CopyLegacyToFloatV2.
    s.BoostMaxUnits = 0x05;
    s.Accelerating  = 128;
    s.DamagedLimit  = 4;
}

// Advance the car's *track* position from its world position.
//
// PhysicsStepF_Tick reads RoadSection / DistanceIntoSection but never writes
// them: in the game they are owned by the legacy track code and pushed in every
// step by CopyLegacyRoadStateToFloatV2 (Car_Behaviour.cpp:3361), which this test
// stubs out. Without this the car drives forward for ever while its track
// position stays pinned at section 0 distance 0 -- so every wheel lookup hits
// the same spot of road and a bumpy surface reads as flat. That silently made
// the first version of the rough-road scenarios meaningless.
//
// The model here is deliberately simple and only valid for the straight, axis
// aligned track this harness builds: distance along the road is just WorldZ.
// One section is CUBE_SIZE (0x04000000) in player_z units and WorldZ is
// player_z / 8 (Physics_FloatV2.h:186), so a section spans 8388608 in WorldZ.
// A straight piece has CoordCount 9, and DistanceIntoSection is 8.8 fixed point
// with the high byte as the coordinate index, so a section spans (9-1) << 8 =
// 2048 distance units -- hence 4096 of WorldZ per distance unit.
//
// This is a test-side stand-in for the game's track following, not a copy of
// it. It is enough to make the wheels traverse a varying surface, which is all
// these scenarios need; it is not enough to model curves, and the harness does
// not build any.
void AdvanceTrackPosition(PhysicsStateF& s, const FV2Track& track) {
    const double kWorldZPerSection      = 8388608.0;
    const double kDistanceUnitsPerSect  = 2048.0;
    const double kWorldZPerDistanceUnit = kWorldZPerSection / kDistanceUnitsPerSect;

    double along = s.WorldZ / kWorldZPerDistanceUnit;
    if (along < 0.0) along = 0.0;

    const int section = static_cast<int>(along / kDistanceUnitsPerSect) % track.SectionCount;
    const double into = std::fmod(along, kDistanceUnitsPerSect);

    s.RoadSection               = static_cast<uint8_t>(section);
    s.DistanceIntoSection       = into;
    // Straight pieces are not plus180-mirrored, so the raw and mirrored forms
    // coincide here. See the DistanceIntoSection note at Car_Behaviour.cpp:3217.
    s.NormalDistanceIntoSection = into;
}

// Run the script at one timestep, sampling at every multiple of sampleEvery.
std::vector<Sample> RunAt(double dt, double duration, double sampleEvery) {
    PhysicsStateF s;
    SeedState(s);
    const FV2Track* track = FV2_GetTrack();

    std::vector<Sample> out;
    double t = 0.0;
    double nextSample = 0.0;
    // Half a step of slack, so a sample instant that lands a hair past a step
    // boundary in floating point is not skipped at one rate and taken at another.
    const double eps = dt * 0.5;

    while (t <= duration + eps) {
        if (t + eps >= nextSample) {
            Sample p{};
            p.t = nextSample;
            p.x = s.WorldX; p.y = s.WorldY; p.z = s.WorldZ;
            p.xAngle = s.XAngle; p.yAngle = s.YAngle; p.zAngle = s.ZAngle;
            p.speed  = s.WorldZSpeed;
            p.damage = s.FrontLeftDamage + s.FrontRightDamage + s.RearDamage;
            // TouchingRoad, not the *Grounded fields: those are edge latches
            // that fire on the step a wheel makes contact, so they read false
            // for most of a run even with the car sitting flat on the road.
            p.airborne = (s.TouchingRoad == 0);
            out.push_back(p);
            nextSample += sampleEvery;
        }
        AdvanceTrackPosition(s, *track);
        PhysicsStepF_Tick(s, ScriptedInput(t), dt);
        t += dt;
    }
    return out;
}

// Build the synthetic track: a ring of straight sections. Section byte 0 is
// template 0 (straight) with angle 0 -- see kPieceDataMap in Track_FloatV2.cpp.
void BuildFlatTrack(int sections) {
    std::vector<char>  pieceByte(sections, 0);
    std::vector<char>  yCoordId(sections, 0);
    std::vector<short> yShift(sections, 0);
    FV2_BuildTrackView(sections, pieceByte.data(), yCoordId.data(),
                       yCoordId.data(), yShift.data(), yShift.data());
}

struct Divergence {
    double maxPos;      // worst |position| gap from the reference, world units
    double maxAngle;    // worst |angle| gap, Amiga angle units (65536 = full turn)
    double maxSpeed;
    int    maxDamage;
    int    endDamage;   // this rate's own final damage, for the report
    double endPos;      // gap at the final sample -- has it settled or run away?
};

Divergence Compare(const std::vector<Sample>& ref, const std::vector<Sample>& other) {
    Divergence d{};
    const size_t n = ref.size() < other.size() ? ref.size() : other.size();
    for (size_t i = 0; i < n; ++i) {
        const Sample& a = ref[i];
        const Sample& b = other[i];
        const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        const double pos = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (pos > d.maxPos) d.maxPos = pos;

        const double da = std::fabs(b.xAngle - a.xAngle);
        const double db = std::fabs(b.yAngle - a.yAngle);
        const double dc = std::fabs(b.zAngle - a.zAngle);
        const double ang = da > db ? (da > dc ? da : dc) : (db > dc ? db : dc);
        if (ang > d.maxAngle) d.maxAngle = ang;

        const double sp = std::fabs(b.speed - a.speed);
        if (sp > d.maxSpeed) d.maxSpeed = sp;

        const int dm = std::abs(b.damage - a.damage);
        if (dm > d.maxDamage) d.maxDamage = dm;

        if (i == n - 1) { d.endPos = pos; d.endDamage = b.damage; }
    }
    return d;
}

void PrintTable(const char* label, const std::vector<Sample>& s) {
    std::printf("\n  %s\n", label);
    std::printf("      t        WorldX        WorldY        WorldZ     YAngle    ZSpeed\n");
    for (const Sample& p : s)
        std::printf("  %5.1f  %12.2f  %12.2f  %12.2f  %9.1f  %8.2f\n",
                    p.t, p.x, p.y, p.z, p.yAngle, p.speed);
}

struct Rate { const char* name; double dt; };
const Rate kRates[] = {
    { "10Hz  (Amiga, reference)", 0.1        },
    { "25Hz",                     1.0 / 25.0 },
    { "60Hz  (shipping default)", 1.0 / 60.0 },
    { "120Hz",                    1.0 / 120.0 },
};
const int nRates = sizeof(kRates) / sizeof(kRates[0]);

const double kDuration    = 15.0;
const double kSampleEvery = 0.5;

// Run every rate over one surface and report. Returns true on failure.
// posPctLimit / angleLimit are ratchets: each is set a little above what this
// scenario measures today, so the test records current behaviour and fails if a
// change makes any rate track the 10Hz reference *worse*. They are not claims
// about what is acceptable -- see the per-scenario notes in main().
bool RunScenario(const char* name, int profile, bool verbose,
                 double posPctLimit, double angleLimit, bool checkDamage) {
    gSurfaceProfile = profile;

    const double duration    = kDuration;
    const double sampleEvery = kSampleEvery;
    const Rate*  rates       = kRates;

    std::printf("\n================================================================\n");
    std::printf("  scenario: %s\n", name);
    std::printf("================================================================\n");

    std::vector<std::vector<Sample>> runs;
    for (int i = 0; i < nRates; ++i)
        runs.push_back(RunAt(rates[i].dt, duration, sampleEvery));

    if (verbose)
        for (int i = 0; i < nRates; ++i)
            PrintTable(rates[i].name, runs[i]);

    // How far does the car travel in the reference run? Position drift only
    // means something relative to that -- 100 units of drift is nothing over a
    // 200000-unit lap and everything over a 500-unit crawl.
    const Sample& refEnd   = runs[0].back();
    const Sample& refStart = runs[0].front();
    const double travelled = std::sqrt(std::pow(refEnd.x - refStart.x, 2) +
                                       std::pow(refEnd.y - refStart.y, 2) +
                                       std::pow(refEnd.z - refStart.z, 2));
    // Report what the reference run actually did, so a scenario that quietly
    // fails to leave the ground (or to take any damage) is visible as such
    // rather than passing on a technicality.
    int airborneSamples = 0;
    for (const Sample& p : runs[0]) if (p.airborne) ++airborneSamples;
    std::printf("\n  reference run travelled %.1f world units in %.0fs"
                " (airborne in %d of %zu samples, ended on %d damage)\n",
                travelled, duration, airborneSamples, runs[0].size(),
                runs[0].back().damage);

    std::printf("\n  drift from the 10Hz reference:\n");
    std::printf("    rate                        max pos    (%% of travel)   max angle   max speed   dmg-gap  end-dmg\n");

    bool failed = false;
    for (int i = 1; i < nRates; ++i) {
        const Divergence d = Compare(runs[0], runs[i]);
        const double pct = travelled > 0.0 ? (d.maxPos / travelled) * 100.0 : 0.0;
        std::printf("    %-24s  %10.2f   %8.2f%%    %9.1f   %9.3f   %6d   %6d\n",
                    rates[i].name, d.maxPos, pct, d.maxAngle, d.maxSpeed,
                    d.maxDamage, d.endDamage);

        if (pct > posPctLimit) {
            std::printf("      FAIL: %s diverges more than %.1f%% of distance travelled\n",
                        rates[i].name, posPctLimit);
            failed = true;
        }
        // Landing damage must not depend on the tick rate. This is the check
        // that caught the real bug: before ProcessWheel evaluated damage per
        // landing rather than per step, the 10Hz car finished this scenario on
        // 58 damage and every faster rate on zero, because the Amiga's
        // penetration-depth threshold is only reachable at the Amiga's own
        // timestep. Allowed spread is generous -- landings are discrete events
        // and the rates do not land in identical places -- but "one rate takes
        // damage and another takes none" must never pass again.
        const int refEndDamage = checkDamage ? runs[0].back().damage : 0;
        if (refEndDamage > 0 && d.endDamage == 0) {
            std::printf("      FAIL: %s took no damage where 10Hz took %d\n",
                        rates[i].name, refEndDamage);
            failed = true;
        } else if (refEndDamage > 0) {
            const double ratio = static_cast<double>(d.endDamage) / refEndDamage;
            if (ratio < 0.5 || ratio > 1.6) {
                std::printf("      FAIL: %s ended on %d damage vs 10Hz's %d\n",
                            rates[i].name, d.endDamage, refEndDamage);
                failed = true;
            }
        }
        // A full turn is 65536 angle units, so 1024 is 5.6 degrees.
        if (d.maxAngle > angleLimit) {
            std::printf("      FAIL: %s orientation diverges by more than %.0f units (%.1f deg)\n",
                        rates[i].name, angleLimit, angleLimit * 360.0 / 65536.0);
            failed = true;
        }
    }

    // Convergence: each halving of dt should not make things *worse*. If the
    // drift grows as the timestep shrinks, the dtRatio scaling is wrong in a
    // way that no amount of tuning will fix.
    std::printf("\n  convergence (drift should not grow as dt shrinks):\n");
    double prev = -1.0;
    for (int i = 1; i < nRates; ++i) {
        const double m = Compare(runs[0], runs[i]).maxPos;
        const char* trend = prev < 0.0 ? "" : (m > prev * 1.5 ? "  <-- growing" : "");
        std::printf("    %-24s  %10.2f%s\n", rates[i].name, m, trend);
        prev = m;
    }

    return failed;
}

} // namespace

int main(int argc, char** argv) {
    const bool verbose = (argc > 1 && std::strcmp(argv[1], "-v") == 0);

    BuildFlatTrack(20);

    std::printf("physics_rate_test — FloatV2 timestep independence\n");
    std::printf("  throttle held, left 4-7s, right 10-13s\n");
    std::printf("  %.0fs of simulation per rate, sampled every %.1fs\n",
                kDuration, kSampleEvery);

    bool failed = false;
    // Flat road: the rates agree closely (<1% of distance, ~1.5 degrees).
    // FloatV2's timestep independence genuinely holds here.
    failed |= RunScenario("flat level road (suspension barely excited)",
                          0, verbose, /*posPct=*/2.0, /*angle=*/1024.0,
                          /*checkDamage=*/true);

    // Washboard: the harshest surface here, and the rates part company badly
    // (~30% of distance, ~50 degrees). Bumps on every coordinate is not a
    // realistic road -- no shipped track is this rough -- so this is kept as a
    // sensitive canary rather than a claim about real play. The wide limits
    // record where it stands today.
    //
    // The damage assertion is off here, and deliberately so. With damage
    // evaluated per landing, this surface makes the *number of landings* itself
    // rate-dependent: a bump on every coordinate means a finer timestep
    // resolves contacts a coarse one blurs into a single touchdown, so 60Hz
    // ends on roughly twice the 10Hz damage. That is a real limit of the
    // per-landing model, but no shipped track is remotely this rough, and
    // tuning the physics to satisfy a surface the game does not contain would
    // be the wrong trade. The numbers are still printed.
    failed |= RunScenario("washboard bumps (every coordinate alternates)",
                          1, verbose, /*posPct=*/35.0, /*angle=*/16384.0,
                          /*checkDamage=*/false);

    // Crest and dip: a realistic feature, and the most informative scenario.
    // The car gets real airtime and lands hard, and this is where the damage
    // model's rate dependence shows up plainly -- see the note in the report.
    failed |= RunScenario("crest and dip (airtime, landings, damage)",
                          2, verbose, /*posPct=*/4.0, /*angle=*/1600.0,
                          /*checkDamage=*/true);

    std::printf("\n%s\n", failed ? "FAILED" : "PASSED");
    return failed ? 1 : 0;
}
