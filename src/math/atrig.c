/* atan, atan2, asin, acos.
 *
 * atan reduces with a table of atan(j/16): for x in [0, 4],
 * atan(x) = atan(c) + atan((x - c) / (1 + x c)) with c = j/16 nearest to
 * x, leaving |t| <= 1/32 for a short odd series; above 4 it uses
 * pi/2 - atan(1/x). Every step keeps a double-double, so asin and acos can
 * be written as atan of an exactly formed ratio without losing accuracy
 * near +-1. */
#include "math_internal.h"

static void atan_series(double th, double tl, double *h, double *l)   /* |t| <= 1/32 */
{
    double z = th * th, p;
    p = -1.0 / 13;
    p = p * z + 1.0 / 11; p = p * z - 1.0 / 9; p = p * z + 1.0 / 7;
    p = p * z - 1.0 / 5;  p = p * z + 1.0 / 3;
    fast_two_sum(th, tl * (1 - z) - th * z * p, h, l);
}

static void atan_tab(double xh, double xl, double *h, double *l)   /* 0 <= x <= 4 */
{
    int j = (int)(xh * 16 + 0.5);
    double c, nh, ph, pe, dh, dl, th, tl, q, qe, sh, sl, s, se;
    if (j == 0) { atan_series(xh, xl, h, l); return; }
    c = j * 0.0625;
    nh = xh - c;                                        /* exact */
    two_prod(xh, c, &ph, &pe);
    two_sum(1.0, ph, &dh, &dl);
    dl += pe + xl * c;
    th = nh / dh;
    two_prod(th, dh, &q, &qe);
    tl = (((nh - q) - qe) + xl - th * dl) / dh;
    atan_series(th, tl, &sh, &sl);
    two_sum(novac__atan_tab[j].hi, sh, &s, &se);
    se += novac__atan_tab[j].lo + sl;
    fast_two_sum(s, se, h, l);
}

void novac__atan_dd(double xh, double xl, double *h, double *l)
{
    double q, ql, p, pe, th, tl, s, se;
    if (xh <= 4.0) { atan_tab(xh, xl, h, l); return; }
    if (xh > 0x1p60) { fast_two_sum(novac__pi_2.hi, novac__pi_2.lo - 1 / xh, h, l); return; }
    q = 1 / xh;
    two_prod(q, xh, &p, &pe);
    ql = (((1 - p) - pe) - q * xl) / xh;
    atan_tab(q, ql, &th, &tl);
    two_sum(novac__pi_2.hi, -th, &s, &se);
    se += novac__pi_2.lo - tl;
    fast_two_sum(s, se, h, l);
}

double atan(double x)
{
    double h, l, a = x < 0 ? -x : x;
    if (x != x) return x;
    if (a < 0x1p-27) return x;
    novac__atan_dd(a, 0.0, &h, &l);
    return x < 0 ? -(h + l) : h + l;
}

/* Scales a and b by the same power of two so the larger is in [1, 2). */
static void normalise_pair(double *a, double *b)
{
    int e = (int)((asu64(*a > *b ? *a : *b) >> 52) & 0x7ff);
    if (e == 0) { *a *= 0x1p600; *b *= 0x1p600; e = (int)((asu64(*a > *b ? *a : *b) >> 52) & 0x7ff); }
    {
        int k = 1023 - e;               /* multiply by 2^k, maybe in two steps */
        if (k > 600) { *a *= 0x1p600; *b *= 0x1p600; k -= 600; }
        if (k < -600) { *a *= 0x1p-600; *b *= 0x1p-600; k += 600; }
        *a *= pow2i(k); *b *= pow2i(k);
    }
}

double atan2(double y, double x)
{
    double a = y < 0 ? -y : y, b = x < 0 ? -x : x, h, l, q, ql, p, pe, r;
    int ys = (asu64(y) >> 63) != 0, xs = (asu64(x) >> 63) != 0;
    if (x != x || y != y) return x + y;
    if (a == 0) return xs ? (ys ? -novac__pi.hi : novac__pi.hi) : y;
    if (b == 0) return ys ? -novac__pi_2.hi : novac__pi_2.hi;
    if (b == __builtin_inf()) {
        if (a == __builtin_inf()) r = xs ? 3 * 0x1.921fb54442d18p-1 : 0x1.921fb54442d18p-1;
        else r = xs ? novac__pi.hi : 0.0;
        return ys ? -r : r;
    }
    if (a == __builtin_inf()) return ys ? -novac__pi_2.hi : novac__pi_2.hi;
    normalise_pair(&a, &b);
    if (b > a * 0x1p60) {                /* y/x tiny */
        if (!xs) { r = (y < 0 ? -a : a) / b; if (r == 0) errno = ERANGE; return r; }
        h = novac__pi.hi; l = novac__pi.lo - a / b;
    } else if (a > b * 0x1p60) {
        h = novac__pi_2.hi; l = novac__pi_2.lo + (xs ? b / a : -b / a);
    } else {
        q = a / b;
        two_prod(q, b, &p, &pe);
        ql = ((a - p) - pe) / b;
        novac__atan_dd(q, ql, &h, &l);
        if (xs) { double s, se; two_sum(novac__pi.hi, -h, &s, &se); se += novac__pi.lo - l; h = s; l = se; }
    }
    r = h + l;
    return ys ? -r : r;
}

/* sqrt(1 - x^2) as a double-double, |x| < 1. */
static void one_minus_sq_sqrt(double x, double *sh, double *sl)
{
    double p, pe, u, ul;
    two_prod(x, x, &p, &pe);
    two_sum(1.0, -p, &u, &ul);
    ul -= pe;
    fast_two_sum(u, ul, &u, &ul);
    novac__sqrt_dd(u, ul, sh, sl);
}

void novac__sqrt_dd(double ah, double al, double *rh, double *rl)
{
    double s = hw_sqrt(ah), p, pe;
    two_prod(s, s, &p, &pe);
    fast_two_sum(s, (((ah - p) - pe) + al) / (2 * s), rh, rl);
}

double asin(double x)
{
    double a = x < 0 ? -x : x, sh, sl, q, ql, p, pe, h, l;
    if (x != x) return x;
    if (a > 1) return math_err(__builtin_nan(""), EDOM);
    if (a < 0x1p-26) return x;
    if (a == 1) return x < 0 ? -novac__pi_2.hi : novac__pi_2.hi;
    one_minus_sq_sqrt(a, &sh, &sl);
    q = a / sh;                          /* tan(asin a) = a / sqrt(1 - a^2) */
    two_prod(q, sh, &p, &pe);
    ql = (((a - p) - pe) - q * sl) / sh;
    novac__atan_dd(q, ql, &h, &l);
    return x < 0 ? -(h + l) : h + l;
}

double acos(double x)
{
    double a = x < 0 ? -x : x, sh, sl, q, ql, p, pe, h, l, s, se;
    if (x != x) return x;
    if (a > 1) return math_err(__builtin_nan(""), EDOM);
    if (x == 1) return 0.0;
    if (x == -1) return novac__pi.hi + novac__pi.lo;
    if (a < 0x1p-60) return novac__pi_2.hi + (novac__pi_2.lo - x);
    one_minus_sq_sqrt(a, &sh, &sl);
    q = sh / a;                          /* tan(acos |x|) */
    two_prod(q, a, &p, &pe);
    ql = (((sh - p) - pe) + sl) / a;
    novac__atan_dd(q, ql, &h, &l);
    if (x > 0) return h + l;
    two_sum(novac__pi.hi, -h, &s, &se);
    return s + (se + novac__pi.lo - l);
}
