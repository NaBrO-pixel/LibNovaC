/* sin, cos, tan.
 *
 * Argument reduction finds n and r with x = n*pi/2 + r, |r| <= pi/4, r as
 * a double-double good to about 2^-100 relative even for the worst cases:
 *   |x| < 2^20 * pi/2  Cody-Waite with pi/2 in four pieces, the first two
 *                      short enough that n * piece is exact;
 *   larger             Payne-Hanek: x * 2/pi mod 4 from a 192-bit window
 *                      of 2/pi's binary expansion, chosen by x's exponent.
 * The kernels are Taylor polynomials on |r| <= pi/4, carried far enough
 * (to r^19 and r^20) that truncation is below 2^-60. */
#include "math_internal.h"

#define SHIFT 0x1.8p52
#define INV_PIO2 0x1.45f306dc9c883p-1
#define PIO2_1 0x1.921fb54400000p+0      /* 33 bits */
#define PIO2_2 0x1.0b4611a600000p-34     /* 33 bits */
#define PIO2_3 0x1.3198a2e037073p-69
#define PIO2_4 0x1.129024e088a68p-123

static uint64_t tbits(long pos)          /* 64 bits of 2/pi from bit pos (0-based) */
{
    long w, sh;
    uint64_t hi, lo;
    if (pos <= -64) return 0;
    if (pos < 0) return tbits(0) >> (-pos);
    w = pos / 64; sh = pos % 64;
    if (w >= 22) return 0;
    hi = novac__two_over_pi[w];
    lo = w + 1 < 22 ? novac__two_over_pi[w + 1] : 0;
    return sh ? (hi << sh) | (lo >> (64 - sh)) : hi;
}

static int payne_hanek(double ax, double *rh, double *rl)
{
    uint64_t u = asu64(ax), M = (u & 0x000fffffffffffffULL) | (1ULL << 52);
    int E = (int)(u >> 52) - 1075;       /* ax = M * 2^E */
    long p = (long)(E - 1) - 1;          /* 0-based position of bit b_(E-1) */
    uint64_t w0 = tbits(p), w1 = tbits(p + 64), w2 = tbits(p + 128);
    unsigned __int128 t;
    uint64_t p0, p1, p2, g0, g1, g2;
    int n;
    double a1, a2, a3, fh, fl, s, se;

    t = (unsigned __int128)M * w2;       p0 = (uint64_t)t;
    t = (unsigned __int128)M * w1 + (uint64_t)(t >> 64); p1 = (uint64_t)t;
    t = (unsigned __int128)M * w0 + (uint64_t)(t >> 64); p2 = (uint64_t)t;
    /* P = p2:p1:p0 = frac(ax / 2pi) * 2^192. Top two bits: the quadrant. */
    n = (int)(p2 >> 62);
    g2 = (p2 << 2) | (p1 >> 62);
    g1 = (p1 << 2) | (p0 >> 62);
    g0 = p0 << 2;
    (void)g0;
    if (g2 >> 63) n++;                   /* fraction >= 1/2: round to nearest quadrant */
    /* G (signed) / 2^192 as an exact sum of three doubles */
    a1 = (double)((int64_t)g2 >> 11) * 0x1p-53;
    a2 = (double)(((g2 & 0x7ff) << 42) | (g1 >> 22)) * 0x1p-106;
    a3 = (double)(g1 & 0x3fffff) * 0x1p-128;
    two_sum(a1, a2, &s, &se);
    se += a3;
    fast_two_sum(s, se, &fh, &fl);
    dd_mul(fh, fl, novac__pi_2.hi, novac__pi_2.lo, rh, rl);
    return n & 3;
}

/* x = n*pi/2 + (rh + rl); returns n mod 4. */
static int rem_pio2(double x, double *rh, double *rl)
{
    double ax = x < 0 ? -x : x;
    if (ax <= 0x1.921fb54442d18p-1) { *rh = x; *rl = 0; return 0; }
    if (ax < 0x1.921fb54442d18p20) {
        double kd = x * INV_PIO2 + SHIFT, t, s, e, q, qe, a, ae, lo;
        int n = (int)(int64_t)asu64(kd);
        kd -= SHIFT;
        t = x - kd * PIO2_1;                      /* exact */
        two_sum(t, -kd * PIO2_2, &s, &e);          /* kd * PIO2_2 exact */
        two_prod(kd, PIO2_3, &q, &qe);
        two_sum(s, -q, &a, &ae);
        lo = ae + e - qe - kd * PIO2_4;
        fast_two_sum(a, lo, rh, rl);
        return n & 3;
    }
    {
        int n = payne_hanek(ax, rh, rl);
        if (x < 0) { *rh = -*rh; *rl = -*rl; n = -n; }
        return n & 3;
    }
}

static void sin_k(double rh, double rl, double *h, double *l)
{
    double z = rh * rh, v = z * rh, p, tail;
    p = -1.0 / 121645100408832000;                                   /* -1/19! */
    p = p * z + 1.0 / 355687428096000;   p = p * z - 1.0 / 1307674368000;
    p = p * z + 1.0 / 6227020800;        p = p * z - 1.0 / 39916800;
    p = p * z + 1.0 / 362880;            p = p * z - 1.0 / 5040;
    p = p * z + 1.0 / 120;
    tail = v * (-1.0 / 6) + (v * z * p + rl * (1 - 0.5 * z));
    fast_two_sum(rh, tail, h, l);
}

static void cos_k(double rh, double rl, double *h, double *l)
{
    double zh, zl, z, hz, w, c, p;
    two_prod(rh, rh, &zh, &zl);
    z = zh;
    p = 1.0 / 2432902008176640000;                                   /* 1/20! */
    p = p * z - 1.0 / 6402373705728000;  p = p * z + 1.0 / 20922789888000;
    p = p * z - 1.0 / 87178291200;       p = p * z + 1.0 / 479001600;
    p = p * z - 1.0 / 3628800;           p = p * z + 1.0 / 40320;
    p = p * z - 1.0 / 720;               p = p * z + 1.0 / 24;
    c = z * z * p;
    hz = 0.5 * zh;
    w = 1 - hz;
    fast_two_sum(w, (((1 - w) - hz) - 0.5 * zl) + (c - rh * rl), h, l);
}

static int bad_arg(double x, double *r)
{
    if (x != x) { *r = x; return 1; }
    if (x == __builtin_inf() || x == -__builtin_inf()) { *r = math_err(__builtin_nan(""), EDOM); return 1; }
    return 0;
}

double sin(double x)
{
    double rh, rl, h, l, r;
    int n;
    if (bad_arg(x, &r)) return r;
    if (x > -0x1p-26 && x < 0x1p-26) return x;
    n = rem_pio2(x, &rh, &rl);
    if (n & 1) cos_k(rh, rl, &h, &l); else sin_k(rh, rl, &h, &l);
    return (n & 2) ? -(h + l) : h + l;
}

double cos(double x)
{
    double rh, rl, h, l, r;
    int n;
    if (bad_arg(x, &r)) return r;
    if (x > -0x1p-27 && x < 0x1p-27) return 1.0;
    n = rem_pio2(x, &rh, &rl);
    if (n & 1) sin_k(rh, rl, &h, &l); else cos_k(rh, rl, &h, &l);
    return (n == 1 || n == 2) ? -(h + l) : h + l;
}

void sincos(double x, double *s, double *c)
{
    double rh, rl, sh, sl, ch, cl, r;
    int n;
    if (bad_arg(x, &r)) { *s = *c = r; return; }
    if (x > -0x1p-27 && x < 0x1p-27) { *s = x; *c = 1.0; return; }
    n = rem_pio2(x, &rh, &rl);
    sin_k(rh, rl, &sh, &sl);
    cos_k(rh, rl, &ch, &cl);
    switch (n) {
    case 0: *s = sh + sl;    *c = ch + cl;    break;
    case 1: *s = ch + cl;    *c = -(sh + sl); break;
    case 2: *s = -(sh + sl); *c = -(ch + cl); break;
    default: *s = -(ch + cl); *c = sh + sl;   break;
    }
}

double tan(double x)
{
    double rh, rl, sh, sl, ch, cl, nh, nl, dh, dl, q, p, pe;
    int n;
    if (bad_arg(x, &q)) return q;
    if (x > -0x1p-27 && x < 0x1p-27) return x;
    n = rem_pio2(x, &rh, &rl);
    sin_k(rh, rl, &sh, &sl);
    cos_k(rh, rl, &ch, &cl);
    if (n & 1) { nh = -ch; nl = -cl; dh = sh; dl = sl; }
    else       { nh = sh;  nl = sl;  dh = ch; dl = cl; }
    q = nh / dh;
    two_prod(q, dh, &p, &pe);
    return q + ((((nh - p) - pe) + nl - q * dl) / dh);
}
