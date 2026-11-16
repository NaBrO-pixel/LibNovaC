/* sinh, cosh, tanh and their inverses. The forward functions use the
 * double-double exp kernel, so (e^x -+ e^-x) loses nothing to
 * cancellation; small arguments take Taylor series instead. */
#include "math_internal.h"

/* e^a as a double-double, 0 <= a <= 40 */
static void exp_pos(double a, double *eh, double *el)
{
    int k;
    double h, l, s;
    novac__exp_dd(a, 0.0, &k, &h, &l);
    s = pow2i(k);
    *eh = h * s; *el = l * s;
}

/* (e + sign/e) / 2 for e = e^a as a double-double */
static double half_sum_inv(double eh, double el, double sign)
{
    double ih = 1 / eh, p, pe, il, s, se;
    two_prod(ih, eh, &p, &pe);
    il = (((1 - p) - pe) - ih * el) / eh;
    two_sum(eh, sign * ih, &s, &se);
    se += el + sign * il;
    return 0.5 * (s + se);
}

double sinh(double x)
{
    double a = x < 0 ? -x : x, r, eh, el;
    if (x != x || a == __builtin_inf()) return x;
    if (a < 0x1p-27) return x;
    if (a <= 1) {                         /* x + x^3/3! + ... + x^19/19! */
        double z = a * a, p = 1.0 / 121645100408832000;
        p = p * z + 1.0 / 355687428096000; p = p * z + 1.0 / 1307674368000;
        p = p * z + 1.0 / 6227020800;      p = p * z + 1.0 / 39916800;
        p = p * z + 1.0 / 362880;          p = p * z + 1.0 / 5040;
        p = p * z + 1.0 / 120;             p = p * z + 1.0 / 6;
        r = a + a * z * p;
    } else if (a <= 40) {
        exp_pos(a, &eh, &el);
        r = half_sum_inv(eh, el, -1.0);
    } else {
        int k;
        double h, l;
        if (a > 710.5) return math_err(x < 0 ? -__builtin_inf() : __builtin_inf(), ERANGE);
        novac__exp_dd(a, 0.0, &k, &h, &l);
        r = novac__scale(h, l, k - 1);
        if (r == __builtin_inf()) errno = ERANGE;
    }
    return x < 0 ? -r : r;
}

double cosh(double x)
{
    double a = x < 0 ? -x : x, eh, el;
    if (x != x) return x;
    if (a == __builtin_inf()) return a;
    if (a < 0x1p-27) return 1.0;
    if (a <= 1) {                         /* 1 + x^2/2! + ... + x^20/20! */
        double z = a * a, p = 1.0 / 2432902008176640000;
        p = p * z + 1.0 / 6402373705728000; p = p * z + 1.0 / 20922789888000;
        p = p * z + 1.0 / 87178291200;      p = p * z + 1.0 / 479001600;
        p = p * z + 1.0 / 3628800;          p = p * z + 1.0 / 40320;
        p = p * z + 1.0 / 720;              p = p * z + 1.0 / 24;
        p = p * z + 0.5;
        return 1 + z * p;
    }
    if (a <= 40) {
        exp_pos(a, &eh, &el);
        return half_sum_inv(eh, el, 1.0);
    }
    {
        int k;
        double h, l, r;
        if (a > 710.5) return math_err(__builtin_inf(), ERANGE);
        novac__exp_dd(a, 0.0, &k, &h, &l);
        r = novac__scale(h, l, k - 1);
        if (r == __builtin_inf()) errno = ERANGE;
        return r;
    }
}

double tanh(double x)
{
    double a = x < 0 ? -x : x, eh, el, nh, nl, dh, dl, q, p, pe, r;
    if (x != x) return x;
    if (a < 0x1p-27) return x;
    if (a > 22) return x < 0 ? -1.0 : 1.0;
    exp_pos(2 * a, &eh, &el);            /* tanh = (e^2a - 1) / (e^2a + 1) */
    two_sum(eh, -1.0, &nh, &nl); nl += el;
    two_sum(eh, 1.0, &dh, &dl);  dl += el;
    q = nh / dh;
    two_prod(q, dh, &p, &pe);
    r = q + ((((nh - p) - pe) + nl - q * dl) / dh);
    return x < 0 ? -r : r;
}

double asinh(double x)
{
    double a = x < 0 ? -x : x, r;
    if (x != x || a == __builtin_inf()) return x;
    if (a < 0x1p-27) return x;
    if (a > 0x1p28) r = log(a) + novac__ln2.hi;
    else if (a > 2) r = log(2 * a + 1 / (hw_sqrt(a * a + 1) + a));
    else { double z = a * a; r = log1p(a + z / (1 + hw_sqrt(1 + z))); }
    return x < 0 ? -r : r;
}

double acosh(double x)
{
    if (x != x) return x;
    if (x < 1) return math_err(__builtin_nan(""), EDOM);
    if (x == __builtin_inf()) return x;
    if (x > 0x1p28) return log(x) + novac__ln2.hi;
    if (x > 2) return log(2 * x - 1 / (x + hw_sqrt(x * x - 1)));
    { double t = x - 1; return log1p(t + hw_sqrt(2 * t + t * t)); }
}

double atanh(double x)
{
    double a = x < 0 ? -x : x, r;
    if (x != x) return x;
    if (a > 1) return math_err(__builtin_nan(""), EDOM);
    if (a == 1) return math_err(x < 0 ? -__builtin_inf() : __builtin_inf(), ERANGE);
    if (a < 0x1p-28) return x;
    if (a < 0.5) r = 0.5 * log1p(2 * a + 2 * a * a / (1 - a));
    else r = 0.5 * log1p(2 * a / (1 - a));
    return x < 0 ? -r : r;
}
