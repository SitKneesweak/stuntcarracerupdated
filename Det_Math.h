// Det_Math.h — bit-for-bit reproducible replacements for the libm functions
// the physics uses.
//
// WHY THIS EXISTS: lockstep multiplayer needs two machines running the same
// inputs to produce the same doubles, forever. IEEE-754 guarantees that for
// +, -, *, / and sqrt, but it says nothing about sin/cos/pow — those are not
// required to be correctly rounded, and glibc, Apple's libm and MSVCRT really
// do disagree in the last ulp. One ulp is all it takes: the physics feeds its
// own output back in every step, so any difference compounds until the two
// cars are in different places.
//
// So everything here is built from arithmetic that IEEE-754 pins down exactly:
// exact range reduction, then a fixed-length polynomial with coefficients
// written as hex-float literals (decimal literals are converted by the
// compiler, hex ones name the bit pattern directly). No table is built at
// startup — a table filled in by std::sin would smuggle the problem back in.
//
// Measured against a long-double reference over a full turn: sin/cos are
// within 7.3e-16 absolute (~3 ulp, most of it the one rounding in converting
// the reduced angle to radians), Exp2-based pow within 2.3e-16 relative
// (~1 ulp). libm is correctly rounded and so slightly better — the point is
// not accuracy, it is that these give *the same* answer everywhere, and 3 ulp
// is far below anything the physics can feel (the road itself is quantised at
// 1/256 of a segment). Anything reached from PhysicsStepF_Tick must use these.
//
// std::sqrt is deliberately NOT wrapped: IEEE-754 mandates it correctly
// rounded, so it is already reproducible.

#ifndef DET_MATH_H
#define DET_MATH_H

#include <cmath>
#include <cstdint>
#include <cstring>

namespace scr {
namespace det {

// One turn == 65536, the Amiga's angle unit.
constexpr double kAngleToRadians = 0x1.921fb54442d18p-14;  // 2*pi/65536

// log2 of the decay bases used by the physics, so pow(base, e) can be done as
// Exp2(log2(base) * e) with no call to a library pow.
constexpr double kLog2_119_128 = -0x1.aed391ab6674ep-4;   // log2(119/128)
constexpr double kLog2_3_4     = -0x1.a8ff971810a5ep-2;   // log2(3/4)
// log2(1/2) is exactly -1, so pow(0.5, e) is just Exp2(-e).

// --- 2^k for small integral k, built from the exponent field ---------------
// Exact for |k| < 1023, and independent of any library.
inline double Ldexp2(int k) {
    uint64_t bits = static_cast<uint64_t>(k + 1023) << 52;
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

// --- sin/cos --------------------------------------------------------------
// Angles arrive already in 65536-per-turn units, so range reduction is exact
// and no Payne-Hanek style fallback is needed: fmod by 65536 is exact (IEEE
// requires fmod to be exact), and subtracting a multiple of 16384 from a value
// of comparable magnitude is exact too. That leaves a quadrant index and a
// remainder in [-8192, 8192] units == [-pi/4, pi/4] radians, where short
// Taylor series are already good to well under an ulp.

struct SinCosPair { double s, c; };

inline SinCosPair SinCosUnits(double angleUnits) {
    double a = std::fmod(angleUnits, 65536.0);        // exact
    // Nearest quadrant. floor(x + 0.5) rather than nearbyint/rint, which
    // follow the current rounding mode.
    double n = std::floor(a * (1.0 / 16384.0) + 0.5); // 1/16384 is a power of 2
    double r = a - n * 16384.0;                       // exact, |r| <= 8192
    double x = r * kAngleToRadians;                   // |x| <= pi/4

    const double x2 = x * x;

    // sin x = x - x^3/3! + ... - x^17/17!  (first omitted term ~1e-19 here)
    double s = x + x * (x2 * (-0x1.5555555555555p-3 + x2 * ( 0x1.1111111111111p-7
                     + x2 * (-0x1.a01a01a01a01ap-13 + x2 * ( 0x1.71de3a556c734p-19
                     + x2 * (-0x1.ae64567f544e4p-26 + x2 * ( 0x1.6124613a86d09p-33
                     + x2 * (-0x1.ae7f3e733b81fp-41 + x2 *   0x1.952c77030ad4ap-49))))))));

    // cos x = 1 - x^2/2! + ... + x^18/18!
    double c = 1.0 + x2 * (-0x1.0000000000000p-1 + x2 * ( 0x1.5555555555555p-5
                    + x2 * (-0x1.6c16c16c16c17p-10 + x2 * ( 0x1.a01a01a01a01ap-16
                    + x2 * (-0x1.27e4fb7789f5cp-22 + x2 * ( 0x1.1eed8eff8d898p-29
                    + x2 * (-0x1.93974a8c07c9dp-37 + x2 * ( 0x1.ae7f3e733b81fp-45
                    + x2 *  -0x1.6827863b97d97p-53))))))));

    // Rotate by the quadrant: angle == n*(pi/2) + x.
    int q = static_cast<int>(n) & 3;
    switch (q) {
        case 0:  return { s,  c };
        case 1:  return { c, -s };
        case 2:  return { -s, -c };
        default: return { -c,  s };
    }
}

inline double SinUnits(double angleUnits) { return SinCosUnits(angleUnits).s; }
inline double CosUnits(double angleUnits) { return SinCosUnits(angleUnits).c; }

// --- 2^x ------------------------------------------------------------------
// Split into an integer part (exact, applied via the exponent field) and a
// fraction in [-0.5, 0.5], then exp() of a small argument.
inline double Exp2(double x) {
    double k = std::floor(x + 0.5);
    double f = x - k;                                  // |f| <= 0.5
    double y = f * 0x1.62e42fefa39efp-1;               // * ln 2, |y| <= 0.347

    // e^y = 1 + y + y^2/2! + ... + y^15/15!
    double e = 1.0 + y * (1.0 + y * (0x1.0000000000000p-1 + y * (0x1.5555555555555p-3
                   + y * (0x1.5555555555555p-5 + y * (0x1.1111111111111p-7
                   + y * (0x1.6c16c16c16c17p-10 + y * (0x1.a01a01a01a01ap-13
                   + y * (0x1.a01a01a01a01ap-16 + y * (0x1.71de3a556c734p-19
                   + y * (0x1.27e4fb7789f5cp-22 + y * (0x1.ae64567f544e4p-26
                   + y * (0x1.1eed8eff8d898p-29 + y * (0x1.6124613a86d09p-33
                   + y *  0x1.ae7f3e733b81fp-41)))))))))))));

    return e * Ldexp2(static_cast<int>(k));
}

// pow(base, exponent) where log2(base) is one of the constants above. The
// physics only ever raises a fixed base to a per-tick exponent, so this covers
// every use without needing a general pow.
inline double PowFromLog2(double log2Base, double exponent) {
    return Exp2(log2Base * exponent);
}

}  // namespace det
}  // namespace scr

#endif  // DET_MATH_H
