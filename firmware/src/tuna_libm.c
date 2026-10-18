/* SPDX-License-Identifier: GPL-3.0-only */
/* TUNA's maths for the FM-1: the five single-precision functions Tuna's C uses (exp2f, sinf, sqrtf, lrintf,
 * fabsf), since the firmware links no C library (Felucca itself has no floating point at all). Built into the
 * firmware and the browser build; the host tests use the system's libm, and tests/tuna_libm_test.c holds these
 * to within a few units in the last place of it.
 * Single precision only: the AC79's FPU has no doubles (and there is no soft-float library to fall back on). */
#include <stdint.h>

typedef union { float f; uint32_t u; } tl_bits;

float fabsf(float x)
{
    tl_bits b = {x};
    b.u &= 0x7FFFFFFFu;
    return b.f;
}

/* to the nearest integer, ties to even (as lrintf in the default rounding mode): 1.5 x 2^23 added and taken
 * away leaves the float rounded by the FPU itself */
long lrintf(float x)
{
    const float big = 12582912.0f;                 /* 1.5 x 2^23 */
    if (fabsf(x) >= 4194304.0f)                    /* 2^22: already whole */
        return (long)x;
    volatile float t = x + big;                    /* (volatile: no folding of the two adds) */
    return (long)(t - big);
}

float sqrtf(float x)
{
    if (!(x > 0.0f))
        return x == 0.0f ? x : (x - x) / (x - x);  /* 0 stays 0; negative or NaN: NaN */
    tl_bits b = {x};
    b.u = 0x1FBD1DF5u + (b.u >> 1);                /* a first guess within a few percent */
    float y = b.f;
    y = 0.5f * (y + x / y);                        /* Newton: each step doubles the bits right */
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

/* 2^x: the nearest whole number into the exponent, 2^f for the rest (|f| <= 1/2) by its series to the 8th
 * power (relative error under 1e-7) */
float exp2f(float x)
{
    if (x != x)
        return x;
    if (x >= 128.0f)
        return 3.4028235e38f * 2.0f;               /* +inf */
    if (x < -126.0f)
        return 0.0f;                               /* (no subnormals needed here) */
    long i = lrintf(x);
    float f = (x - (float)i) * 0.69314718056f;     /* 2^f = e^(f ln 2) */
    float p = 2.4801587302e-5f;                    /* 1 / 8! */
    p = p * f + 1.9841269841e-4f;
    p = p * f + 1.3888888889e-3f;
    p = p * f + 8.3333333333e-3f;
    p = p * f + 4.1666666667e-2f;
    p = p * f + 1.6666666667e-1f;
    p = p * f + 0.5f;
    p = p * f + 1.0f;
    p = p * f + 1.0f;
    tl_bits b;
    b.u = (uint32_t)(i + 127) << 23;
    return p * b.f;
}

/* sin x: reduced to [-pi, pi] by whole turns, folded into [-pi/2, pi/2], an odd polynomial (degree 11) */
float sinf(float x)
{
    const float two_pi = 6.28318530718f, pi = 3.14159265359f, half_pi = 1.57079632679f;
    if (x != x)
        return x;
    float k = x * (1.0f / two_pi);
    long n = lrintf(k);
    float r = x - (float)n * two_pi;               /* (Tuna's angles are small: one subtraction is enough) */
    if (r > half_pi)
        r = pi - r;
    else if (r < -half_pi)
        r = -pi - r;
    float r2 = r * r;
    float p = -2.5052108385e-8f;
    p = p * r2 + 2.7557319224e-6f;
    p = p * r2 - 1.9841269841e-4f;
    p = p * r2 + 8.3333333333e-3f;
    p = p * r2 - 1.6666666667e-1f;
    return r + r * r2 * p;
}
