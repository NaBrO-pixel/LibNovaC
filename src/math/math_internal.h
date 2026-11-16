/* Shared machinery for libNovaC's <math.h>.
 *
 * Accuracy comes from double-double ("dd") arithmetic: a value carried as
 * an unevaluated sum hi + lo of two doubles, worth about 106 bits. The
 * error-free transformations below are exact provided the compiler does
 * not contract a*b+c into an FMA (the Makefile passes -ffp-contract=off)
 * and arithmetic is IEEE double (SSE2 on x86-64, never x87). Every
 * function assumes the default round-to-nearest mode. */
#ifndef NOVAC_MATH_INTERNAL_H
#define NOVAC_MATH_INTERNAL_H
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>

struct novac_dd { double hi, lo; };

extern const struct novac_dd novac__exp2_tab[64];
extern const double novac__log_invc[128];
extern const struct novac_dd novac__log_tab[128];
extern const struct novac_dd novac__atan_tab[65];
extern const struct novac_dd novac__ln2, novac__inv_ln2, novac__inv_ln10,
                             novac__log10_2, novac__pi, novac__pi_2;
extern const uint64_t novac__two_over_pi[22];

static inline uint64_t asu64(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }
static inline double asdouble(uint64_t u) { double x; memcpy(&x, &u, 8); return x; }
static inline uint32_t asu32(float x) { uint32_t u; memcpy(&u, &x, 4); return u; }
static inline float asfloat(uint32_t u) { float x; memcpy(&x, &u, 4); return x; }

/* Keeps a value the compiler might otherwise fold or reorder. */
static inline double opaque(double x) { __asm__ volatile ("" : "+x"(x)); return x; }

/* s + e == a + b exactly. */
static inline void two_sum(double a, double b, double *s, double *e)
{
    double t = a + b, bb = t - a;
    *s = t;
    *e = (a - (t - bb)) + (b - bb);
}
/* Same, when |a| >= |b| (or a == 0). */
static inline void fast_two_sum(double a, double b, double *s, double *e)
{
    double t = a + b;
    *s = t;
    *e = b - (t - a);
}
/* Dekker: hi + lo == a with each half holding at most 26 bits.
 * Valid for |a| < 2^996. */
static inline void split(double a, double *hi, double *lo)
{
    double c = 134217729.0 * a;              /* 2^27 + 1 */
    *hi = c - (c - a);
    *lo = a - *hi;
}
/* p + e == a * b exactly (barring overflow and underflow). */
static inline void two_prod(double a, double b, double *p, double *e)
{
    double ah, al, bh, bl;
    *p = a * b;
    split(a, &ah, &al);
    split(b, &bh, &bl);
    *e = ((ah * bh - *p) + ah * bl + al * bh) + al * bl;
}
/* (ah + al) * (bh + bl), to about 104 bits. */
static inline void dd_mul(double ah, double al, double bh, double bl, double *rh, double *rl)
{
    double p, e;
    two_prod(ah, bh, &p, &e);
    e += ah * bl + al * bh;
    fast_two_sum(p, e, rh, rl);
}

/* 2^k as a double, for -1022 <= k <= 1023. */
static inline double pow2i(int k) { return asdouble((uint64_t)(k + 1023) << 52); }

static inline double math_err(double v, int e) { errno = e; return v; }

/* Internal kernels shared between the source files. */
/* log(x) for finite x > 0 as hi + lo, relative error about 2^-68. */
void   novac__log_dd(double x, double *hi, double *lo);
/* exp(hi + lo) for |hi| < 710: returns 2^k * (rh + rl) with *k. */
void   novac__exp_dd(double hi, double lo, int *k, double *rh, double *rl);
/* Scales hi + lo by 2^k with one rounding (or two only in the subnormal
 * range). Handles overflow to inf and underflow to 0. */
double novac__scale(double hi, double lo, int k);
/* atan(xh + xl) for xh >= 0, as a double-double. */
void   novac__atan_dd(double xh, double xl, double *rh, double *rl);
/* sqrt(ah + al) as a double-double, ah > 0. */
void   novac__sqrt_dd(double ah, double al, double *rh, double *rl);

static inline double hw_sqrt(double x)
{
    __asm__ ("sqrtsd %1, %0" : "=x"(x) : "x"(x));
    return x;
}
static inline float hw_sqrtf(float x)
{
    __asm__ ("sqrtss %1, %0" : "=x"(x) : "x"(x));
    return x;
}
#endif
