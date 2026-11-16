/* The exact functions of <math.h>: rounding, decomposition, remainders,
 * comparisons, sqrt, cbrt and hypot. Everything here is either exact or
 * correctly rounded (cbrt and hypot: within an ulp). */
#include "math_internal.h"
#include <limits.h>

double fabs(double x) { return asdouble(asu64(x) & ~(1ULL << 63)); }
double copysign(double x, double y)
{
    return asdouble((asu64(x) & ~(1ULL << 63)) | (asu64(y) & (1ULL << 63)));
}
double nan(const char *tag) { (void)tag; return __builtin_nan(""); }

double trunc(double x)
{
    uint64_t u = asu64(x);
    int e = (int)((u >> 52) & 0x7ff) - 1023;
    if (e >= 52) return x;               /* integral, inf or nan */
    if (e < 0) return asdouble(u & (1ULL << 63));
    return asdouble(u & ~((1ULL << (52 - e)) - 1));
}

double floor(double x)
{
    double t = trunc(x);
    return (x < t) ? t - 1 : t;           /* only negatives with a fraction */
}

double ceil(double x)
{
    double t = trunc(x);
    if (x > t) return t + 1;
    return t;
}

double round(double x)                    /* halfway cases away from zero */
{
    double t = trunc(x), d = x - t;       /* exact */
    if (d >= 0.5) return t + 1;
    if (d <= -0.5) return t - 1;
    return t;
}

double rint(double x)                     /* current rounding mode */
{
    double c = copysign(0x1p52, x), r;
    if (!(fabs(x) < 0x1p52)) return x;
    r = opaque(x + c) - c;               /* rounds the signed value, in the current mode */
    return copysign(r, x);               /* keeps the sign of a zero result */
}

static inline unsigned get_mxcsr(void) { unsigned m; __asm__ volatile ("stmxcsr %0" : "=m"(m)); return m; }
static inline void set_mxcsr(unsigned m) { __asm__ volatile ("ldmxcsr %0" : : "m"(m)); }

double nearbyint(double x)                /* rint without raising inexact */
{
    unsigned m = get_mxcsr();
    double r = rint(x);
    set_mxcsr(m);
    return r;
}

static long long to_ll(double r)
{
    if (!(r >= -0x1p63 && r < 0x1p63)) { errno = EDOM; return LLONG_MIN; }
    return (long long)r;
}
long      lrint(double x)   { return (long)to_ll(rint(x)); }
long long llrint(double x)  { return to_ll(rint(x)); }
long      lround(double x)  { return (long)to_ll(round(x)); }
long long llround(double x) { return to_ll(round(x)); }

double modf(double x, double *ip)
{
    double t = trunc(x);
    *ip = t;
    if (fabs(x) == __builtin_inf()) return copysign(0.0, x);
    return copysign(x - t, x);
}

double frexp(double x, int *e)
{
    uint64_t u = asu64(x);
    int ee = (int)((u >> 52) & 0x7ff);
    if (ee == 0) {
        if (x == 0) { *e = 0; return x; }
        x *= 0x1p64;
        u = asu64(x);
        ee = (int)((u >> 52) & 0x7ff) - 64;
    } else if (ee == 0x7ff) { *e = 0; return x; }
    *e = ee - 1022;
    return asdouble((u & ~(0x7ffULL << 52)) | (1022ULL << 52));
}

/* x * 2^n with a single rounding (in the subnormal range too). */
double scalbn(double x, int n)
{
    if (n > 1023) {
        x *= 0x1p1023; n -= 1023;
        if (n > 1023) { x *= 0x1p1023; n -= 1023; if (n > 1023) n = 1023; }
    } else if (n < -1022) {
        /* 2^-1022 * 2^53: keeps the intermediate normal so the last step rounds once */
        x *= 0x1p-1022 * 0x1p53; n += 1022 - 53;
        if (n < -1022) {
            x *= 0x1p-1022 * 0x1p53; n += 1022 - 53;
            if (n < -1022) n = -1022;
        }
    }
    x *= pow2i(n);
    if (x == 0 || fabs(x) == __builtin_inf()) errno = ERANGE;
    return x;
}
double scalbln(double x, long n)
{
    if (n > INT_MAX) n = INT_MAX;
    if (n < INT_MIN) n = INT_MIN;
    return scalbn(x, (int)n);
}
double ldexp(double x, int n)
{
    if (x == 0 || x != x || fabs(x) == __builtin_inf()) return x;
    return scalbn(x, n);
}

int ilogb(double x)
{
    uint64_t u = asu64(x);
    int e = (int)((u >> 52) & 0x7ff);
    if (e == 0) {
        if ((u << 1) == 0) { errno = EDOM; return FP_ILOGB0; }
        return ilogb(x * 0x1p64) - 64;
    }
    if (e == 0x7ff) { errno = EDOM; return (u << 12) ? FP_ILOGBNAN : INT_MAX; }
    return e - 1023;
}
double logb(double x)
{
    if (x == 0) return math_err(-__builtin_inf(), ERANGE);
    if (x != x) return x;
    if (fabs(x) == __builtin_inf()) return __builtin_inf();
    return (double)ilogb(x);
}

double nextafter(double x, double y)
{
    uint64_t u;
    if (x != x || y != y) return x + y;
    if (x == y) return y;
    if (x == 0) return copysign(asdouble(1), y);
    u = asu64(x);
    if ((y > x) == (x > 0)) u++; else u--;
    x = asdouble(u);
    if (fabs(x) == __builtin_inf() || fabs(x) < 0x1p-1022) errno = ERANGE;
    return x;
}
double nexttoward(double x, long double y)
{
    if (y != y) return (double)y;
    if ((long double)x == y) return (double)y;
    return nextafter(x, (long double)x < y ? __builtin_inf() : -__builtin_inf());
}

double fdim(double x, double y)
{
    if (x != x || y != y) return x + y;
    return x > y ? x - y : 0.0;
}
double fmax(double x, double y)
{
    if (x != x) return y;
    if (y != y) return x;
    if (x == 0 && y == 0) return (asu64(x) >> 63) ? y : x;
    return x > y ? x : y;
}
double fmin(double x, double y)
{
    if (x != x) return y;
    if (y != y) return x;
    if (x == 0 && y == 0) return (asu64(x) >> 63) ? x : y;
    return x < y ? x : y;
}

/* ---- remainders: exact, by long division of the significands ---------- */

/* |x| = mx * 2^ex with mx in [2^52, 2^53) (subnormals normalised) */
static void unpack(double x, uint64_t *m, int *e)
{
    uint64_t u = asu64(x) & ~(1ULL << 63);
    int ee = (int)(u >> 52);
    uint64_t mm = u & ((1ULL << 52) - 1);
    if (ee == 0) { ee = 1; while (!(mm & (1ULL << 52))) { mm <<= 1; ee--; } }
    else mm |= 1ULL << 52;
    *m = mm; *e = ee - 1075;
}
static double pack(uint64_t m, int e)     /* exact: m * 2^e is representable */
{
    return m ? scalbn((double)m, e) : 0.0;
}

/* |x| mod |y| and the low bits of the quotient; x, y finite, y != 0. */
static double fmod_core(double x, double y, uint64_t *quo)
{
    uint64_t mx, my, q = 0;
    int ex, ey;
    double ax = fabs(x), ay = fabs(y);
    *quo = 0;
    if (ax < ay) return ax;
    unpack(ax, &mx, &ex);
    unpack(ay, &my, &ey);
    for (; ex > ey; ex--) {
        q <<= 1;
        if (mx >= my) { mx -= my; q |= 1; }
        mx <<= 1;
    }
    q <<= 1;
    if (mx >= my) { mx -= my; q |= 1; }
    *quo = q;
    return pack(mx, ey);
}

double fmod(double x, double y)
{
    uint64_t q;
    double r;
    if (x != x || y != y) return x + y;
    if (y == 0 || fabs(x) == __builtin_inf()) return math_err(__builtin_nan(""), EDOM);
    if (fabs(y) == __builtin_inf()) return x;
    r = fmod_core(x, y, &q);
    return copysign(r, x);
}

double remquo(double x, double y, int *quo)
{
    uint64_t q;
    double r, ay = fabs(y);
    int neg = (asu64(x) >> 63) != (asu64(y) >> 63);
    *quo = 0;
    if (x != x || y != y) return x + y;
    if (y == 0 || fabs(x) == __builtin_inf()) return math_err(__builtin_nan(""), EDOM);
    if (ay == __builtin_inf()) return x;
    r = fmod_core(x, y, &q);              /* 0 <= r < |y| */
    /* to the nearest multiple; ties to an even quotient */
    if (ay < 0x1p1022 ? (2 * r > ay || (2 * r == ay && (q & 1))) : (r > 0.5 * ay || (r == 0.5 * ay && (q & 1)))) {
        r -= ay; q++;
    }
    *quo = (int)(q & 0x7fffffff) * (neg ? -1 : 1);
    if (r == 0) r = 0.0;
    return (asu64(x) >> 63) ? -r : r;
}

double remainder(double x, double y) { int q; return remquo(x, y, &q); }

/* ---- roots --------------------------------------------------------------- */

double sqrt(double x)
{
    if (x < 0) return math_err(__builtin_nan(""), EDOM);
    return hw_sqrt(x);
}

double cbrt(double x)
{
    double a = fabs(x), t, th, tl, ch, cl, r, s = 1;
    uint64_t u;
    if (x != x || a == 0 || a == __builtin_inf()) return x + x;
    if (a < 0x1p-1000) { a *= 0x1p150; s = 0x1p-50; }
    else if (a > 0x1p1000) { a *= 0x1p-150; s = 0x1p50; }   /* keep t^3 finite */
    u = asu64(a);
    t = asdouble(u / 3 + 0x2a9f7893782da1ceULL);    /* ~5 bits */
    t = t - (t * t * t - a) / (3 * t * t);           /* Newton: 10, 20, 40, 53 bits */
    t = t - (t * t * t - a) / (3 * t * t);
    t = t - (t * t * t - a) / (3 * t * t);
    t = t - (t * t * t - a) / (3 * t * t);
    /* final correction from the exact residual a - t^3 */
    two_prod(t, t, &th, &tl);
    dd_mul(th, tl, t, 0.0, &ch, &cl);
    r = ((a - ch) - cl) / (3 * t * t);
    t = (t + r) * s;
    return x < 0 ? -t : t;
}

double hypot(double x, double y)
{
    double a = fabs(x), b = fabs(y), t, ah, al, bh, bl, sh, sl, rh, rl, scale = 1;
    if (a == __builtin_inf() || b == __builtin_inf()) return __builtin_inf();
    if (x != x || y != y) return x + y;
    if (a < b) { t = a; a = b; b = t; }
    if (b == 0) return a;
    if (a > b * 0x1p60) return a + b;
    if (a > 0x1p500) { a *= 0x1p-600; b *= 0x1p-600; scale = 0x1p600; }
    else if (b < 0x1p-500) { a *= 0x1p600; b *= 0x1p600; scale = 0x1p-600; }
    two_prod(a, a, &ah, &al);
    two_prod(b, b, &bh, &bl);
    two_sum(ah, bh, &sh, &sl);
    sl += al + bl;
    fast_two_sum(sh, sl, &sh, &sl);
    novac__sqrt_dd(sh, sl, &rh, &rl);
    t = (rh + rl) * scale;
    if (t == __builtin_inf()) errno = ERANGE;
    return t;
}
