/* <math.h> against the host's libm (glibc): each function on random
 * arguments across its interesting ranges, measured in ulps. glibc's own
 * results are within an ulp of exact (mostly correctly rounded), so a
 * bound of 1 ulp against it means at most about 1.5 ulps from the truth. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fenv.h>
#include "harness.h"
#include "oracle.h"

static uint64_t rng = 0x2545F4914F6CDD1DULL;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static double unif(double lo, double hi) { return lo + (hi - lo) * (double)(next() >> 11) * 0x1p-53; }
/* log-uniform magnitude in [2^lo, 2^hi), random sign if sgn */
static double logu(int lo, int hi, int sgn)
{
    double m = 1 + (double)(next() >> 12) * 0x1p-52;
    double v = ldexp(m, lo + (int)(next() % (unsigned)(hi - lo)));
    return (sgn && (next() & 1)) ? -v : v;
}
static uint64_t bits(double x) { uint64_t u; memcpy(&u, &x, 8); return u; }

static double ulp_diff(double a, double b)
{
    int64_t ia, ib;
    if (a != a && b != b) return 0;
    if (a != a || b != b) return 1e9;
    if (a == b) return 0;
    ia = (int64_t)bits(a); ib = (int64_t)bits(b);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return (double)(ia > ib ? ia - ib : ib - ia);
}

typedef double (*f1_t)(double);
typedef double (*f2_t)(double, double);
static const struct { const char *name; f1_t f; double tol; } fn1[] = {
    { "exp", exp, 1 }, { "exp2", exp2, 1 }, { "expm1", expm1, 1 }, { "log", log, 1 },
    { "log2", log2, 1 }, { "log10", log10, 2 },   /* glibc's log10 errs by up to 2 ulp */ { "log1p", log1p, 1 }, { "sin", sin, 1 },
    { "cos", cos, 1 }, { "tan", tan, 1 }, { "asin", asin, 1 }, { "acos", acos, 1 },
    { "atan", atan, 1 }, { "sinh", sinh, 2 }, { "cosh", cosh, 2 }, { "tanh", tanh, 2 },
    { "asinh", asinh, 2 }, { "acosh", acosh, 2 }, { "atanh", atanh, 2 }, { "sqrt", sqrt, 0 },
    { "cbrt", cbrt, 4 },   /* glibc's cbrt is up to 4 ulp out; exactness is checked below */ { "floor", floor, 0 }, { "ceil", ceil, 0 }, { "trunc", trunc, 0 },
    { "round", round, 0 }, { "rint", rint, 0 }, { "nearbyint", nearbyint, 0 }, { "logb", logb, 0 },
    { "fabs", fabs, 0 },
};
static const struct { const char *name; f2_t f; double tol; } fn2[] = {
    { "pow", pow, 1 }, { "atan2", atan2, 1 }, { "hypot", hypot, 1 }, { "fmod", fmod, 0 },
    { "remainder", remainder, 0 }, { "fdim", fdim, 0 }, { "fmax", fmax, 0 }, { "fmin", fmin, 0 },
    { "copysign", copysign, 0 }, { "nextafter", nextafter, 0 },
};

static double arg1(int id)
{
    switch (next() % 5) {
    case 0: return logu(-1074, 1024, 1);                 /* anywhere */
    case 1: return unif(-10, 10);
    case 2: return unif(-1, 1);
    case 3: return logu(-30, 10, 1);
    default:
        switch (id) {
        case 0: case 2: case 13: case 14: return unif(-750, 712);
        case 1: return unif(-1080, 1025);
        case 3: case 4: case 5: return unif(0.9, 1.1);
        case 6: return unif(-0.5, 0.5);
        case 7: case 8: case 9: return (next() & 1) ? unif(-1e6, 1e6) : logu(20, 1024, 1);
        case 17: return unif(1, 3);
        default: return unif(-2, 2);
        }
    }
}

static void check_specials(void)
{
    double inf = INFINITY;
    errno = 0; CHECK(log(0.0) == -inf && errno == ERANGE);
    errno = 0; CHECK(isnan(log(-1.0)) && errno == EDOM);
    errno = 0; CHECK(exp(1000) == inf && errno == ERANGE);
    errno = 0; CHECK(exp(-1000) == 0 && errno == ERANGE);
    errno = 0; CHECK(isnan(sqrt(-1)) && errno == EDOM);
    errno = 0; CHECK(isnan(sin(inf)) && errno == EDOM);
    errno = 0; CHECK(pow(0.0, -1.0) == inf && errno == ERANGE);
    CHECK(pow(-0.0, -3.0) == -inf);
    CHECK(pow(-8.0, 1.0 / 3) != pow(-8.0, 1.0 / 3));       /* NaN */
    CHECK(pow(-2.0, 3.0) == -8.0 && pow(-2.0, 4.0) == 16.0);
    CHECK(pow(NAN, 0.0) == 1.0 && pow(1.0, NAN) == 1.0);
    CHECK(pow(-1.0, inf) == 1.0 && pow(0.5, -inf) == inf && pow(2, -inf) == 0);
    CHECK(pow(-inf, -3.0) == 0 && signbit(pow(-inf, -3.0)) && pow(-inf, 3.0) == -inf);
    CHECK(atan2(0.0, -0.0) == M_PI && atan2(-0.0, -1.0) == -M_PI);
    CHECK(atan2(inf, -inf) == 3 * M_PI_4 && atan2(-inf, inf) == -M_PI_4);
    CHECK(signbit(sin(-0.0)) && signbit(tan(-0.0)) && signbit(atan(-0.0)) && signbit(asinh(-0.0)));
    CHECK(signbit(floor(-0.5)) == 1 && floor(-0.5) == -1 && signbit(ceil(-0.5)) && round(-0.5) == -1);
    CHECK(round(2.5) == 3 && rint(2.5) == 2 && rint(3.5) == 4 && nearbyint(-2.5) == -2);
    CHECK(lround(-2.5) == -3 && llrint(1e15 + 0.5) == 1000000000000000LL);
    CHECK(hypot(inf, NAN) == inf && hypot(3, 4) == 5 && hypot(1e300, 1e300) == 1e300 * sqrt(2));
    CHECK(cbrt(-27.0) == -3.0 && cbrt(1e-310) > 0);
    CHECK(fmax(NAN, 1) == 1 && fmin(1, NAN) == 1 && signbit(fmin(0.0, -0.0)) && !signbit(fmax(-0.0, 0.0)));
    CHECK(isinf(1.0 / 0.0) && isinf(-HUGE_VAL) < 0 && !isfinite(NAN) && isnormal(1.0) &&
          fpclassify(5e-324) == FP_SUBNORMAL && fpclassify(0.0) == FP_ZERO);
    CHECK(exp(0) == 1 && log(1) == 0 && log2(1024) == 10 && log10(1000) == 3 && log10(1e-300) == -300);
    CHECK(exp2(-1074) == 5e-324 && exp2(10) == 1024 && log2(0x1p-1074) == -1074);
    CHECK(sqrt(2) * sqrt(2) != 2 && sqrt(4) == 2);
    CHECK(sinf(1.0f) == (float)0.8414709848078965 && powf(2.0f, 10.0f) == 1024.0f);
    errno = 0; CHECK(expf(100.0f) == INFINITY && errno == ERANGE);
    { int e; double m = frexp(48.0, &e); CHECK(m == 0.75 && e == 6);
      m = frexp(5e-324, &e); CHECK(m == 0.5 && e == -1073); }
    { double ip, fr = modf(-3.75, &ip); CHECK(fr == -0.75 && ip == -3); }
    { int q; double r = remquo(10.0, 3.0, &q); CHECK(r == 1 && q == 3); r = remquo(-7.0, 2.0, &q); CHECK(r == 1 && q == -4); }
    CHECK(scalbn(1, -1074) == 5e-324 && scalbn(0x1p-1000, 2000) == 0x1p1000 && ldexp(3, 2) == 12);
    CHECK(ilogb(0.5) == -1 && ilogb(5e-324) == -1074 && logb(1024) == 10);
    CHECK(nextafter(1.0, 2.0) == 1 + 0x1p-52 && nextafter(0.0, -1.0) == -5e-324 && nextafterf(1.0f, 0.0f) == 1 - 0x1p-24f);
    CHECK(fmod(5.5, 2) == 1.5 && fmod(-5.5, 2) == -1.5 && fmod(1e300, 3) == oracle_m2(3, 1e300, 3));
    { double s, c; sincos(1.0, &s, &c); CHECK(s == sin(1.0) && c == cos(1.0)); }

    /* fenv: rounding modes and exception flags */
    CHECK(fegetround() == FE_TONEAREST);
    CHECK(fesetround(FE_UPWARD) == 0 && fegetround() == FE_UPWARD);
    { volatile double a = 1.0, b = 3.0; double up = a / b; fesetround(FE_DOWNWARD);
      CHECK(up > a / b); CHECK(rint(2.5) == 2 && rint(-2.5) == -3); }
    fesetround(FE_TONEAREST);
    feclearexcept(FE_ALL_EXCEPT);
    { volatile double z = 0.0; volatile double r = 1.0 / z; (void)r; }
    CHECK(fetestexcept(FE_DIVBYZERO) == FE_DIVBYZERO && !fetestexcept(FE_INVALID));
    feclearexcept(FE_ALL_EXCEPT);
    CHECK(fetestexcept(FE_ALL_EXCEPT) == 0);
    { fenv_t env; feholdexcept(&env); feraiseexcept(FE_OVERFLOW); CHECK(fetestexcept(FE_OVERFLOW));
      fesetenv(&env); CHECK(!fetestexcept(FE_OVERFLOW)); }
    feclearexcept(FE_ALL_EXCEPT);
    { double r = nearbyint(2.4); CHECK(r == 2 && !fetestexcept(FE_INEXACT)); }
}

void test_math(void)
{
    size_t i;
    int k;
    check_specials();
    for (i = 0; i < sizeof fn1 / sizeof fn1[0]; i++) {
        double worst = 0, wx = 0;
        for (k = 0; k < 40000; k++) {
            double x = arg1((int)i), got, want, d;
            got = fn1[i].f(x);
            want = oracle_m1((int)i, x);
            d = ulp_diff(got, want);
            if (d > worst) { worst = d; wx = x; }
        }
        if (worst > fn1[i].tol) th_note("  %-9s max %.0f ulp at %a (got %a, want %a)\n", fn1[i].name, worst, wx, fn1[i].f(wx), oracle_m1((int)i, wx));
        CHECK(worst <= fn1[i].tol);
    }
    /* Float forms against glibc's DOUBLE result rounded to float: glibc's
     * own sinhf, tanhf and log10f are 2 ulps out in places. */
    for (i = 0; i < 21; i++) {
        double worst = 0; float wx = 0;
        for (k = 0; k < 20000; k++) {
            float x = (float)arg1((int)i), got, want;
            float (*const ours[])(float) = { expf, exp2f, expm1f, logf, log2f, log10f, log1pf, sinf, cosf,
                tanf, asinf, acosf, atanf, sinhf, coshf, tanhf, asinhf, acoshf, atanhf, sqrtf, cbrtf };
            double d;
            got = ours[i](x); want = (float)oracle_m1((int)i, x);
            d = ulp_diff(got, want);   /* float bits compared as doubles: 1 float ulp = 2^29 double ulps */
            d /= 0x1p29;
            if (d > worst) { worst = d; wx = x; }
        }
        if (worst > 1) th_note("  %sf max %.2f ulp at %a\n", fn1[i].name, worst, (double)wx);
        CHECK(worst <= 1);
    }
    for (i = 0; i < sizeof fn2 / sizeof fn2[0]; i++) {
        double worst = 0, wx = 0, wy = 0;
        for (k = 0; k < 40000; k++) {
            double x, y, got, want, d;
            switch (next() % 4) {
            case 0: x = logu(-1074, 1024, 1); y = logu(-1074, 1024, 1); break;
            case 1: x = unif(-10, 10); y = unif(-10, 10); break;
            case 2: x = logu(-20, 20, 0); y = unif(-300, 300); break;
            default: x = unif(0.5, 2); y = logu(0, 11, 1); break;
            }
            if (i != 0 && (next() & 1)) { x = logu(-60, 60, 1); y = logu(-60, 60, 1); }
            got = fn2[i].f(x, y);
            want = oracle_m2((int)i, x, y);
            d = ulp_diff(got, want);
            if (d > worst) { worst = d; wx = x; wy = y; }
        }
        if (worst > fn2[i].tol) th_note("  %-9s max %.0f ulp at (%a, %a): got %a want %a\n", fn2[i].name, worst, wx, wy, fn2[i].f(wx, wy), oracle_m2((int)i, wx, wy));
        CHECK(worst <= fn2[i].tol);
    }
    /* cbrt of exact cubes is exact */
    for (k = 0; k < 5000; k++) {
        double n = (double)(int64_t)(next() % 2000000) - 1000000;
        CHECK(cbrt(n * n * n) == n);
        n = ldexp(n, (int)(next() % 600) - 300);
        CHECK(cbrt(n * n * n) == n);
    }
    /* integer powers stay exact where the result is representable */
    for (k = 0; k < 2000; k++) {
        double x = (double)(int)(next() % 2000) - 1000;
        int n = (int)(next() % 6);
        double want = 1;
        int j;
        for (j = 0; j < n; j++) want *= x;
        CHECK(pow(x, n) == want);
    }
}
