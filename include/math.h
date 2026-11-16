/* <math.h>: C11 mathematics for double, float and long double.
 *
 * The double functions are within one ulp of the exact result (most are
 * correctly rounded nearly always), handle C's Annex F special cases, and
 * report errors through errno (EDOM, ERANGE). They assume the default
 * round-to-nearest mode. The float forms are computed in double; the long
 * double forms are computed at double precision. Not provided: fma, erf,
 * erfc, tgamma, lgamma, the Bessel functions. */
#ifndef NOVAC_MATH_H
#define NOVAC_MATH_H
#include <novac/defs.h>

typedef float  float_t;
typedef double double_t;

#define HUGE_VAL  __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()
#define INFINITY  __builtin_inff()
#define NAN       __builtin_nanf("")

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4
#define FP_ILOGB0    (-2147483647 - 1)
#define FP_ILOGBNAN  (-2147483647 - 1)

#define MATH_ERRNO     1
#define MATH_ERREXCEPT 2
#define math_errhandling MATH_ERRNO

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isnan(x)      __builtin_isnan(x)
#define isinf(x)      __builtin_isinf_sign(x)
#define isfinite(x)   __builtin_isfinite(x)
#define isnormal(x)   __builtin_isnormal(x)
#define signbit(x)    __builtin_signbit(x)
#define isgreater(x, y)      __builtin_isgreater(x, y)
#define isgreaterequal(x, y) __builtin_isgreaterequal(x, y)
#define isless(x, y)         __builtin_isless(x, y)
#define islessequal(x, y)    __builtin_islessequal(x, y)
#define islessgreater(x, y)  __builtin_islessgreater(x, y)
#define isunordered(x, y)    __builtin_isunordered(x, y)

#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440

NOVAC_BEGIN_DECLS
#define NOVAC_MATH_1(n) double n(double); float n##f(float); long double n##l(long double);
#define NOVAC_MATH_2(n) double n(double, double); float n##f(float, float); \
                        long double n##l(long double, long double);
NOVAC_MATH_1(exp) NOVAC_MATH_1(exp2) NOVAC_MATH_1(expm1)
NOVAC_MATH_1(log) NOVAC_MATH_1(log2) NOVAC_MATH_1(log10) NOVAC_MATH_1(log1p) NOVAC_MATH_1(logb)
NOVAC_MATH_1(sqrt) NOVAC_MATH_1(cbrt)
NOVAC_MATH_1(sin) NOVAC_MATH_1(cos) NOVAC_MATH_1(tan)
NOVAC_MATH_1(asin) NOVAC_MATH_1(acos) NOVAC_MATH_1(atan)
NOVAC_MATH_1(sinh) NOVAC_MATH_1(cosh) NOVAC_MATH_1(tanh)
NOVAC_MATH_1(asinh) NOVAC_MATH_1(acosh) NOVAC_MATH_1(atanh)
NOVAC_MATH_1(fabs) NOVAC_MATH_1(floor) NOVAC_MATH_1(ceil) NOVAC_MATH_1(trunc)
NOVAC_MATH_1(round) NOVAC_MATH_1(rint) NOVAC_MATH_1(nearbyint)
NOVAC_MATH_2(pow) NOVAC_MATH_2(atan2) NOVAC_MATH_2(hypot) NOVAC_MATH_2(fmod)
NOVAC_MATH_2(remainder) NOVAC_MATH_2(copysign) NOVAC_MATH_2(nextafter)
NOVAC_MATH_2(fdim) NOVAC_MATH_2(fmax) NOVAC_MATH_2(fmin)
#undef NOVAC_MATH_1
#undef NOVAC_MATH_2

double modf(double x, double *ip);       float modff(float x, float *ip);
long double modfl(long double x, long double *ip);
double frexp(double x, int *e);          float frexpf(float x, int *e);
long double frexpl(long double x, int *e);
double ldexp(double x, int n);           float ldexpf(float x, int n);
long double ldexpl(long double x, int n);
double scalbn(double x, int n);          float scalbnf(float x, int n);
long double scalbnl(long double x, int n);
double scalbln(double x, long n);        float scalblnf(float x, long n);
long double scalblnl(long double x, long n);
int ilogb(double x); int ilogbf(float x); int ilogbl(long double x);
double remquo(double x, double y, int *q); float remquof(float x, float y, int *q);
long double remquol(long double x, long double y, int *q);
double nexttoward(double x, long double y); float nexttowardf(float x, long double y);
long double nexttowardl(long double x, long double y);
long lrint(double x);  long lrintf(float x);  long lrintl(long double x);
long long llrint(double x); long long llrintf(float x); long long llrintl(long double x);
long lround(double x); long lroundf(float x); long lroundl(long double x);
long long llround(double x); long long llroundf(float x); long long llroundl(long double x);
double nan(const char *tag); float nanf(const char *tag); long double nanl(const char *tag);
/* GNU: both at once. */
void sincos(double x, double *s, double *c);
void sincosf(float x, float *s, float *c);
NOVAC_END_DECLS
#endif
