/* The float and long double forms of <math.h>.
 *
 * float functions evaluate in double and round once to float: the double
 * result is within an ulp of double, so the float is correctly rounded but
 * for vanishingly rare cases. Overflow and underflow of the narrowing set
 * errno. The long double forms compute at DOUBLE precision (libNovaC has
 * no 80-bit math) and are there so that portable code links. */
#include "math_internal.h"

static float narrow(double r)
{
    float f = (float)r;
    if (r == r && r != __builtin_inf() && r != -__builtin_inf()) {
        if (f == __builtin_inff() || f == -__builtin_inff() || (f == 0 && r != 0)) errno = ERANGE;
    }
    return f;
}

#define F1(n) float n##f(float x) { return narrow(n((double)x)); } \
              long double n##l(long double x) { return (long double)n((double)x); }
#define F2(n) float n##f(float x, float y) { return narrow(n((double)x, (double)y)); } \
              long double n##l(long double x, long double y) { return (long double)n((double)x, (double)y); }

F1(exp) F1(exp2) F1(expm1) F1(log) F1(log2) F1(log10) F1(log1p)
F1(sin) F1(cos) F1(tan) F1(asin) F1(acos) F1(atan)
F1(sinh) F1(cosh) F1(tanh) F1(asinh) F1(acosh) F1(atanh)
F1(cbrt) F1(floor) F1(ceil) F1(trunc) F1(round) F1(rint) F1(nearbyint) F1(logb)
F2(pow) F2(atan2) F2(hypot) F2(fmod) F2(remainder) F2(fdim) F2(fmax) F2(fmin) F2(copysign)

float sqrtf(float x)
{
    if (x < 0) return (float)math_err(__builtin_nan(""), EDOM);
    return hw_sqrtf(x);
}
long double sqrtl(long double x) { return (long double)sqrt((double)x); }

float fabsf(float x) { return asfloat(asu32(x) & 0x7fffffffu); }
long double fabsl(long double x) { return x < 0 || (x == 0 && __builtin_signbit(x)) ? -x : x; }
float nanf(const char *t) { (void)t; return __builtin_nanf(""); }
long double nanl(const char *t) { (void)t; return __builtin_nanl(""); }

long      lrintf(float x)   { return lrint(x); }
long long llrintf(float x)  { return llrint(x); }
long      lroundf(float x)  { return lround(x); }
long long llroundf(float x) { return llround(x); }
long      lrintl(long double x)   { return lrint((double)x); }
long long llrintl(long double x)  { return llrint((double)x); }
long      lroundl(long double x)  { return lround((double)x); }
long long llroundl(long double x) { return llround((double)x); }

float modff(float x, float *ip) { double i, r = modf(x, &i); *ip = (float)i; return (float)r; }
long double modfl(long double x, long double *ip) { double i, r = modf((double)x, &i); *ip = i; return r; }
float frexpf(float x, int *e) { return (float)frexp(x, e); }
long double frexpl(long double x, int *e) { return frexp((double)x, e); }
float ldexpf(float x, int n) { return narrow(ldexp(x, n)); }
long double ldexpl(long double x, int n) { return ldexp((double)x, n); }
float scalbnf(float x, int n) { return narrow(scalbn(x, n)); }
long double scalbnl(long double x, int n) { return scalbn((double)x, n); }
float scalblnf(float x, long n) { return narrow(scalbln(x, n)); }
long double scalblnl(long double x, long n) { return scalbln((double)x, n); }
int ilogbf(float x) { return ilogb(x); }
int ilogbl(long double x) { return ilogb((double)x); }
float remquof(float x, float y, int *q) { return (float)remquo(x, y, q); }
long double remquol(long double x, long double y, int *q) { return remquo((double)x, (double)y, q); }
void sincosf(float x, float *s, float *c) { double a, b; sincos(x, &a, &b); *s = (float)a; *c = (float)b; }

float nextafterf(float x, float y)
{
    uint32_t u;
    if (x != x || y != y) return x + y;
    if (x == y) return y;
    if (x == 0) return asfloat((asu32(y) & 0x80000000u) | 1);
    u = asu32(x);
    if ((y > x) == (x > 0)) u++; else u--;
    x = asfloat(u);
    if (x == __builtin_inff() || x == -__builtin_inff() || fabsf(x) < 0x1p-126f) errno = ERANGE;
    return x;
}
float nexttowardf(float x, long double y)
{
    if (y != y) return (float)y;
    if ((long double)x == y) return (float)y;
    return nextafterf(x, (long double)x < y ? __builtin_inff() : -__builtin_inff());
}
long double nextafterl(long double x, long double y) { return nextafter((double)x, (double)y); }
long double nexttowardl(long double x, long double y) { return nextafter((double)x, (double)y); }
