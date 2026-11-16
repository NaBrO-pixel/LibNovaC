/* log, log2, log10, log1p and the double-double log kernel used by pow.
 *
 * x = 2^k * m, m in [1, 2). With j the top 7 bits of m's fraction and
 * invc ~ 1/(1 + (j + 0.5)/128) from a table, m * invc = 1 + r is formed
 * exactly (Dekker product), |r| < 2^-8, and
 *     log(x) = k ln2 - log(invc) + log1p(r)
 * with -log(invc) stored to 106 bits. Near x = 1 that sum would cancel, so
 * |x - 1| < 1/16 takes a direct series in f = x - 1, which is exact. */
#include "math_internal.h"

/* log1p(f) for |f| < 1/16 as a double-double; f exact. */
static void log1p_small(double f, double *hi, double *lo)
{
    /* f - f^2/2 + f^3 (1/3 - f/4 + ...); terms to f^16 */
    double p = -1.0 / 16, q, h, e, s, se;
    int i;
    for (i = 15; i >= 3; i--) p = p * f + ((i & 1) ? 1.0 : -1.0) / i;
    two_prod(f, f, &h, &e);
    h *= -0.5; e *= -0.5;                 /* -f^2/2, exact */
    q = f * h * -2.0 * p;                 /* f^3 * p */
    two_sum(f, h, &s, &se);
    se += e + q;
    fast_two_sum(s, se, hi, lo);
}

/* log1p(rh + rl) for |r| < 2^-8 as a double-double. */
static void log1p_tiny(double rh, double rl, double *hi, double *lo)
{
    double p, h, e, s, se, r2;
    two_prod(rh, rh, &h, &e);
    r2 = h;
    h *= -0.5; e = e * -0.5 - rh * rl;    /* -(r^2)/2 to about 2^-70 */
    p = rh * r2 * (1.0 / 3 + rh * (-0.25 + rh * (0.2 + rh * (-1.0 / 6 + rh * (1.0 / 7 + rh * (-0.125 + rh / 9))))));
    two_sum(rh, h, &s, &se);
    se += e + rl + p;
    fast_two_sum(s, se, hi, lo);
}

/* log(x) without the k*ln2 term: sets *k and log(m) as hi + lo. */
static void log_parts(double x, int *k, double *hi, double *lo)
{
    uint64_t u = asu64(x);
    int e = (int)(u >> 52);
    double m, invc, ph, pl, rh, rl, lh, ll, s, se;
    int j;
    if (e == 0) {                         /* subnormal: normalise first */
        u = asu64(x * 0x1p54);
        e = (int)(u >> 52) - 54;
    }
    *k = e - 1023;
    u = (u & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL;
    m = asdouble(u);
    j = (int)((u >> 45) & 127);
    invc = novac__log_invc[j];
    two_prod(m, invc, &ph, &pl);
    two_sum(ph - 1.0, pl, &rh, &rl);       /* ph - 1 is exact */
    log1p_tiny(rh, rl, &lh, &ll);
    two_sum(novac__log_tab[j].hi, lh, &s, &se);
    se += novac__log_tab[j].lo + ll;
    fast_two_sum(s, se, hi, lo);
}

void novac__log_dd(double x, double *hi, double *lo)
{
    int k;
    double h, l, a, ae, s, se;
    if (x > 0.9375 && x < 1.0625) { log1p_small(x - 1.0, hi, lo); return; }
    log_parts(x, &k, &h, &l);
    two_prod((double)k, novac__ln2.hi, &a, &ae);
    two_sum(a, h, &s, &se);
    se += ae + l + k * novac__ln2.lo;
    fast_two_sum(s, se, hi, lo);
}

static int log_special(double x, double *r)
{
    if (x != x) { *r = x; return 1; }
    if (x == 0) { *r = math_err(-__builtin_inf(), ERANGE); return 1; }
    if (x < 0) { *r = math_err(__builtin_nan(""), EDOM); return 1; }
    if (x == __builtin_inf()) { *r = x; return 1; }
    return 0;
}

double log(double x)
{
    double r, h, l;
    if (log_special(x, &r)) return r;
    if (x == 1.0) return 0.0;
    novac__log_dd(x, &h, &l);
    return h + l;
}

double log2(double x)
{
    double r, h, l, mh, ml, s, se;
    int k;
    if (log_special(x, &r)) return r;
    if (x > 0.9375 && x < 1.0625) {
        if (x == 1.0) return 0.0;
        log1p_small(x - 1.0, &h, &l);
        dd_mul(h, l, novac__inv_ln2.hi, novac__inv_ln2.lo, &mh, &ml);
        return mh + ml;
    }
    log_parts(x, &k, &h, &l);
    dd_mul(h, l, novac__inv_ln2.hi, novac__inv_ln2.lo, &mh, &ml);
    two_sum((double)k, mh, &s, &se);
    return s + (se + ml);
}

double log10(double x)
{
    double r, h, l, mh, ml, kh, kl, s, se;
    int k;
    if (log_special(x, &r)) return r;
    if (x > 0.9375 && x < 1.0625) {
        if (x == 1.0) return 0.0;
        log1p_small(x - 1.0, &h, &l);
        dd_mul(h, l, novac__inv_ln10.hi, novac__inv_ln10.lo, &mh, &ml);
        return mh + ml;
    }
    log_parts(x, &k, &h, &l);
    dd_mul(h, l, novac__inv_ln10.hi, novac__inv_ln10.lo, &mh, &ml);
    dd_mul((double)k, 0.0, novac__log10_2.hi, novac__log10_2.lo, &kh, &kl);
    two_sum(kh, mh, &s, &se);
    return s + (se + kl + ml);
}

double log1p(double x)
{
    double h, l, u, c;
    if (x != x) return x;
    if (x <= -1.0) {
        if (x == -1.0) return math_err(-__builtin_inf(), ERANGE);
        return math_err(__builtin_nan(""), EDOM);
    }
    if (x == __builtin_inf()) return x;
    if (x > -0.0625 && x < 0.0625) {
        if (x > -0x1p-54 && x < 0x1p-54) return x;
        log1p_small(x, &h, &l);
        return h + l;
    }
    two_sum(1.0, x, &u, &c);              /* 1 + x == u + c exactly */
    novac__log_dd(u, &h, &l);
    return h + (l + c / u);
}
