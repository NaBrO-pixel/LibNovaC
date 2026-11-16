/* exp, exp2, expm1 and the exp kernel used by pow, sinh, cosh and tanh.
 *
 * x = (64k + j) * ln2/64 + r with |r| <= ln2/128, so
 * exp(x) = 2^k * 2^(j/64) * exp(r): the table gives 2^(j/64) to 106 bits
 * and a degree-7 Taylor polynomial gives exp(r) - 1 to well under 2^-64. */
#include "math_internal.h"

#define SHIFT 0x1.8p52                  /* adding it rounds to an integer */
#define INV_LN2_64 0x1.71547652b82fep6  /* 64 / ln 2 */
/* ln2/64 in three pieces; the first two have trailing zero bits so that
 * n * piece is exact for |n| < 2^20. */
#define LN2_64_1 0x1.62e42fefa0000p-7
#define LN2_64_2 0x1.cf79abc9e0000p-46
#define LN2_64_3 0x1.d9cc01f97b57ap-85

static double expm1_poly(double r)     /* exp(r) - 1 - r, |r| <= 0.0055 */
{
    double r2 = r * r;
    return r2 * (0.5 + r * (1.0 / 6 + r * (1.0 / 24 + r * (1.0 / 120 + r * (1.0 / 720 + r * (1.0 / 5040))))));
}

void novac__exp_dd(double hi, double lo, int *k, double *rh, double *rl)
{
    double kd = hi * INV_LN2_64 + SHIFT, r, rr, p, t, e;
    int n;
    n = (int)(int64_t)asu64(kd);         /* low bits of the shifted value */
    kd -= SHIFT;
    /* r = hi - kd*ln2/64 as an exact double-double, then the correction */
    t = hi - kd * LN2_64_1;              /* exact (Sterbenz) */
    two_sum(t, -kd * LN2_64_2, &r, &e);
    rr = e - kd * LN2_64_3 + lo;
    fast_two_sum(r, rr, &r, &rr);
    p = r + (expm1_poly(r) + rr * (1 + r));   /* exp(r) - 1 */
    {
        const struct novac_dd *T = &novac__exp2_tab[n & 63];
        *k = n >> 6;                      /* arithmetic shift: floor division */
        fast_two_sum(T->hi, T->lo + T->hi * p, rh, rl);
    }
}

double novac__scale(double hi, double lo, int k)
{
    if (k > 1023) {                       /* 2^1024 is not a double: split the scaling */
        double v = (hi + lo) * pow2i(k - 1000);
        return v * 0x1p1000;
    }
    if (k >= -1021) return (hi + lo) * pow2i(k);   /* exact scaling of a rounded sum */
    if (k < -1100) return 0.0 * hi;
    /* Subnormal result: add in a way that rounds once at the final scale. */
    {
        double s = pow2i(k + 1000), h = hi * s, l = lo * s;   /* exact */
        double big = 0x1p-1000 * h;       /* h * 2^-1000, rounded once ... */
        double back = big * 0x1p1000;     /* ... and the part it lost */
        return big + (((h - back) + l) * 0x1p-1000);
    }
}

double exp(double x)
{
    int k;
    double h, l;
    if (!(x <= 709.79)) {
        if (x != x) return x;
        if (x == __builtin_inf()) return x;
        return math_err(__builtin_inf(), ERANGE);
    }
    if (x < -746.0) {
        if (x == -__builtin_inf()) return 0.0;
        return math_err(0.0, ERANGE);
    }
    if (x > -0x1p-54 && x < 0x1p-54) return 1.0 + x;
    novac__exp_dd(x, 0.0, &k, &h, &l);
    {
        double v = novac__scale(h, l, k);
        if (v < 0x1p-1022 || v == __builtin_inf()) errno = ERANGE;
        return v;
    }
}

double exp2(double x)
{
    double kd, r, rh, rl, p;
    int n;
    if (!(x < 1024.0)) {
        if (x != x || x == __builtin_inf()) return x;
        return math_err(__builtin_inf(), ERANGE);
    }
    if (x < -1075.0) {
        if (x == -__builtin_inf()) return 0.0;
        return math_err(0.0, ERANGE);
    }
    kd = x * 64.0 + SHIFT;
    n = (int)(int64_t)asu64(kd);
    kd -= SHIFT;
    r = x - kd * (1.0 / 64);              /* exact */
    dd_mul(r, 0.0, novac__ln2.hi, novac__ln2.lo, &rh, &rl);
    p = rh + (expm1_poly(rh) + rl);
    {
        const struct novac_dd *T = &novac__exp2_tab[n & 63];
        double h, l, v;
        fast_two_sum(T->hi, T->lo + T->hi * p, &h, &l);
        v = novac__scale(h, l, n >> 6);
        if (v < 0x1p-1022 || v == __builtin_inf()) errno = ERANGE;
        return v;
    }
}

double expm1(double x)
{
    if (x != x) return x;
    if (x > 709.79) return math_err(__builtin_inf(), ERANGE);
    if (x < -40.0) return -1.0;           /* exp(x) < 2^-57 */
    if (x > -0.34657359 && x < 0.34657359) {
        /* Taylor to x^16: the tail beyond is below 2^-60 of the result */
        double p = 1.0 / 20922789888000;
        static const double c[] = { 1.0 / 1307674368000, 1.0 / 87178291200, 1.0 / 6227020800,
            1.0 / 479001600, 1.0 / 39916800, 1.0 / 3628800, 1.0 / 362880, 1.0 / 40320,
            1.0 / 5040, 1.0 / 720, 1.0 / 120, 1.0 / 24, 1.0 / 6, 0.5 };
        unsigned i;
        if (x > -0x1p-54 && x < 0x1p-54) return x;
        for (i = 0; i < sizeof c / sizeof c[0]; i++) p = p * x + c[i];
        return x + x * x * p;
    }
    {
        int k;
        double h, l, s, a, b, e;
        novac__exp_dd(x, 0.0, &k, &h, &l);
        if (k > 1020) {
            double v = novac__scale(h, l, k);
            if (v == __builtin_inf()) errno = ERANGE;
            return v;
        }
        s = pow2i(k);
        a = h * s; b = l * s;                 /* exact */
        two_sum(a, -1.0, &a, &e);
        return a + (e + b);
    }
}
