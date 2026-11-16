/* strtod, strtof, strtold, atof: correctly rounded (ties to even), for
 * decimal and hexadecimal input, with "inf", "infinity" and "nan(...)".
 *
 * Decimal input is converted exactly: the significant digits become a big
 * integer D and the value D * 10^E is either formed outright (E >= 0) or
 * divided out bit by bit (E < 0), so the one and only rounding happens in
 * novac__round_*. Only the first 800 significant digits are kept; any
 * nonzero digit after them is folded into a trailing 1, which cannot change
 * the rounding because no double needs more than 767 digits to decide it. */
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "../internal/fp.h"
#include "../internal/bigint.h"

#define MAXD 800

typedef struct {
    int neg;
    int kind;                 /* 0 number, 1 inf, 2 nan, 3 no conversion */
    uint64_t m; int e; int sticky;   /* binary result (hex input or big path) */
    int is_zero;
    int range;
} conv_t;

static int lower_eq(const char *s, const char *word)
{
    for (; *word; s++, word++)
        if (tolower((unsigned char)*s) != *word) return 0;
    return 1;
}

static long add_exp(long a, long b)
{
    long r = a + b;
    if (r > 100000) r = 100000;
    if (r < -100000) r = -100000;
    return r;
}

/* Parses an optional exponent ("e+12", "p-3") at *pp. */
static long parse_exp(const char **pp, char mark)
{
    const char *p = *pp;
    int neg = 0;
    long v = 0;
    if (tolower((unsigned char)*p) != mark) return 0;
    p++;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    if (!isdigit((unsigned char)*p)) return 0;      /* "1e" - the 'e' is not part of it */
    while (isdigit((unsigned char)*p)) {
        if (v < 100000) v = v * 10 + (*p - '0');
        p++;
    }
    *pp = p;
    return neg ? -v : v;
}

static void parse_hex(const char **pp, conv_t *c)
{
    const char *p = *pp;
    uint64_t m = 0;
    long e = 0;
    int any = 0, sticky = 0, point = 0;
    for (;; p++) {
        int d;
        if (*p == '.' && !point) { point = 1; continue; }
        if (!isxdigit((unsigned char)*p)) break;
        d = isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10;
        any = 1;
        if (m >> 60) {                    /* full: later digits only matter as sticky */
            if (d) sticky = 1;
            if (!point) e = add_exp(e, 4);
        } else {
            m = m * 16 + (uint64_t)d;
            if (point) e = add_exp(e, -4);
        }
    }
    if (!any) { c->kind = 3; return; }
    e = add_exp(e, parse_exp(&p, 'p'));
    *pp = p;
    if (m == 0) { c->is_zero = 1; return; }
    c->m = m; c->e = (int)e; c->sticky = sticky;
}

/* Exact D * 10^E -> (m, e, sticky). D has nd >= 1 digits, no leading zero. */
static void decimal_to_binary(const char *dig, int nd, long E, conv_t *c)
{
    novac_big D;
    int i = 0;
    novac_big_set_u64(&D, 0);
    while (i < nd) {
        uint32_t chunk = 0, scale = 1;
        int k;
        for (k = 0; k < 9 && i < nd; k++, i++) { chunk = chunk * 10 + (uint32_t)(dig[i] - '0'); scale *= 10; }
        novac_big_mul_small(&D, scale);
        novac_big_add_small(&D, chunk);
    }
    if (E >= 0) {
        unsigned L;
        novac_big_mul_pow10(&D, (unsigned)E);
        L = novac_big_bitlen(&D);
        if (L <= 64) { c->m = novac_big_bits64(&D, 0, NULL); c->e = 0; c->sticky = 0; }
        else { c->m = novac_big_bits64(&D, L - 64, &c->sticky); c->e = (int)L - 64; }
    } else {
        /* q = floor(D * 2^s / 10^-E), chosen so 2^62 < q < 2^64. */
        novac_big Q, T;
        int s, i2;
        uint64_t q = 0;
        novac_big_set_u64(&Q, 1);
        novac_big_mul_pow10(&Q, (unsigned)-E);
        s = (int)novac_big_bitlen(&Q) - (int)novac_big_bitlen(&D) + 63;
        if (s >= 0) novac_big_shl(&D, (unsigned)s);
        else novac_big_shl(&Q, (unsigned)-s);
        novac_big_copy(&T, &Q);
        novac_big_shl(&T, 63);
        for (i2 = 63; i2 >= 0; i2--) {
            if (novac_big_cmp(&D, &T) >= 0) { novac_big_sub(&D, &T); q |= 1ULL << i2; }
            novac_big_shr1(&T);
        }
        c->m = q;
        c->e = -s;
        c->sticky = D.n != 0;
    }
}

/* Fast path: few digits and a small power of ten are both exact doubles,
 * so one IEEE multiply or divide is correctly rounded. */
static const double p10[] = { 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10,
    1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };

static const char *scan(const char *s, conv_t *c, int want_float, double *fast)
{
    const char *p = s;
    char dig[MAXD];
    int nd = 0, any = 0, dropped_nonzero = 0, point = 0;
    long E = 0;

    memset(c, 0, sizeof *c);
    *fast = -1.0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') c->neg = *p++ == '-';

    if (lower_eq(p, "inf")) {
        c->kind = 1;
        p += lower_eq(p, "infinity") ? 8 : 3;
        return p;
    }
    if (lower_eq(p, "nan")) {
        c->kind = 2;
        p += 3;
        if (*p == '(') {
            const char *q = p + 1;
            while (isalnum((unsigned char)*q) || *q == '_') q++;
            if (*q == ')') p = q + 1;
        }
        return p;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
        (isxdigit((unsigned char)p[2]) || (p[2] == '.' && isxdigit((unsigned char)p[3])))) {
        const char *q = p + 2;
        parse_hex(&q, c);
        return q;
    }
    for (;; p++) {
        if (*p == '.' && !point) { point = 1; continue; }
        if (!isdigit((unsigned char)*p)) break;
        any = 1;
        if (*p == '0' && nd == 0) { if (point) E--; continue; }   /* leading zeros */
        if (nd < MAXD - 1) { dig[nd++] = *p; if (point) E--; }
        else { if (*p != '0') dropped_nonzero = 1; if (!point) E++; }
    }
    if (!any) { c->kind = 3; return s; }
    E = add_exp(E, parse_exp(&p, 'e'));
    if (dropped_nonzero) { dig[nd++] = '1'; E--; }
    while (nd > 0 && dig[nd - 1] == '0') { nd--; E++; }
    if (nd == 0) { c->is_zero = 1; return p; }
    if (nd + E > 310) { c->range = 1; c->kind = 1; return p; }
    if (nd + E < -326) { c->range = 1; c->is_zero = 1; return p; }
    if (!want_float && nd <= 15 && E >= -22 && E <= 22) {
        double v = 0;
        int i;
        for (i = 0; i < nd; i++) v = v * 10 + (dig[i] - '0');
        *fast = E >= 0 ? v * p10[E] : v / p10[-E];
        return p;
    }
    decimal_to_binary(dig, nd, E, c);
    return p;
}

static double finish_double(const conv_t *c, double fast)
{
    double v;
    int range = c->range;
    if (c->kind == 1) v = __builtin_inf();
    else if (c->kind == 2) v = __builtin_nan("");
    else if (c->is_zero) v = 0.0;
    else if (fast >= 0) v = fast;
    else v = novac__round_double(c->m, c->e, c->sticky, &range);
    if (range) errno = ERANGE;
    return c->neg ? -v : v;
}

double strtod(const char *restrict s, char **restrict end)
{
    conv_t c;
    double fast;
    const char *p = scan(s, &c, 0, &fast);
    if (c.kind == 3) { if (end) *end = (char *)s; return 0.0; }
    if (end) *end = (char *)p;
    return finish_double(&c, fast);
}

float strtof(const char *restrict s, char **restrict end)
{
    conv_t c;
    double fast;
    float v;
    int range;
    const char *p = scan(s, &c, 1, &fast);
    if (c.kind == 3) { if (end) *end = (char *)s; return 0.0f; }
    if (end) *end = (char *)p;
    range = c.range;
    if (c.kind == 1) v = __builtin_inff();
    else if (c.kind == 2) v = __builtin_nanf("");
    else if (c.is_zero) v = 0.0f;
    else v = novac__round_float(c.m, c.e, c.sticky, &range);
    if (range) errno = ERANGE;
    return c.neg ? -v : v;
}

/* long double is converted through double: correct to double precision,
 * not to the x87 format's 64 bits. */
long double strtold(const char *restrict s, char **restrict end)
{
    return (long double)strtod(s, end);
}

double atof(const char *s) { return strtod(s, NULL); }
