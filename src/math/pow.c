/* pow: exp(y * log(x)) with log carried to about 2^-68 and the product
 * formed exactly, so even results near the overflow threshold stay within
 * an ulp. All of C's special cases (Annex F) are handled first. */
#include "math_internal.h"

/* 0 = not an integer, 1 = odd integer, 2 = even integer */
static int int_kind(double y)
{
    uint64_t u = asu64(y);
    int e = (int)((u >> 52) & 0x7ff) - 1023;
    if (e < 0) return y == 0 ? 2 : 0;
    if (e >= 53) return 2;
    if (u & ((1ULL << (52 - e)) - 1)) return 0;
    if (e == 0) return 1;
    return (u >> (52 - e)) & 1 ? 1 : 2;
}

double pow(double x, double y)
{
    double ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, lh, ll, ph, pl, h, l, v;
    int yk, neg = 0, k;
    const double inf = __builtin_inf();

    if (y == 0 || x == 1) return 1.0;
    if (x != x || y != y) return x + y;
    yk = int_kind(y);
    if (x == 0) {
        int sx = (asu64(x) >> 63) != 0;
        if (y < 0) { errno = ERANGE; return (yk == 1 && sx) ? -inf : inf; }
        return (yk == 1 && sx) ? -0.0 : 0.0;
    }
    if (ay == inf) {
        if (ax == 1) return 1.0;
        return ((ax > 1) == (y > 0)) ? inf : 0.0;
    }
    if (ax == inf) {
        int sx = x < 0;
        if (y < 0) return (yk == 1 && sx) ? -0.0 : 0.0;
        return (yk == 1 && sx) ? -inf : inf;
    }
    if (x < 0) {
        if (yk == 0) return math_err(__builtin_nan(""), EDOM);
        neg = yk == 1;
    }
    if (y == 1) return x;
    if (y == 2) return x * x;
    if (y == -1) return 1 / x;
    if (y == 0.5 && x >= 0) return hw_sqrt(x);

    if (ay > 0x1p900) {                  /* |y log x| is huge unless x == 1 */
        errno = ERANGE;
        return ((ax > 1) == (y > 0)) ? (neg ? -inf : inf) : (neg ? -0.0 : 0.0);
    }
    novac__log_dd(ax, &lh, &ll);
    two_prod(y, lh, &ph, &pl);
    pl += y * ll;
    fast_two_sum(ph, pl, &ph, &pl);
    if (ph > 709.8) { errno = ERANGE; return neg ? -inf : inf; }
    if (ph < -746.0) { errno = ERANGE; return neg ? -0.0 : 0.0; }
    novac__exp_dd(ph, pl, &k, &h, &l);
    v = novac__scale(h, l, k);
    if (v == inf || v < 0x1p-1022) errno = ERANGE;
    return neg ? -v : v;
}
