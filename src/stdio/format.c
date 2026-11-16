/* Formatting engine behind snprintf and friends. Output-agnostic: everything
 * goes through a sink callback.
 *
 * Supported: flags "-+ #0"; width and precision (number or '*'); length
 * modifiers hh h l ll z t j L; conversions d i u o x X c s p % and the
 * floating-point f F e E g G a A. Floating-point output is exact and
 * correctly rounded (ties to even), like glibc's. A long double (L) is
 * printed at double precision.
 * Unsupported (returns -1/EINVAL): %n, wide chars. */
#include <novac/format.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>
#include "../internal/fp.h"

typedef struct {
    novac_sink_fn sink;
    void *ctx;
    size_t total;       /* characters produced so far, always <= INT_MAX */
    int overflow;
} out_t;

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_Z, L_T, L_J, L_BIGL };

typedef struct {
    int minus, plus, space, alt, zero;
    int width;          /* 0 = none */
    int prec;           /* -1 = none */
    int len;
} spec_t;

static void emit(out_t *o, const char *s, size_t n)
{
    if (n == 0 || o->overflow) return;
    if (n > (size_t)INT_MAX - o->total) { o->overflow = 1; return; }
    o->sink(o->ctx, s, n);
    o->total += n;
}

static void emit_rep(out_t *o, char c, size_t n)
{
    char chunk[16];
    memset(chunk, c, sizeof chunk);
    while (n && !o->overflow) {
        size_t k = n < sizeof chunk ? n : sizeof chunk;
        emit(o, chunk, k);
        n -= k;
    }
}

static void emit_padded(out_t *o, const spec_t *s, const char *str, size_t n)
{
    size_t w = (size_t)s->width, pad = w > n ? w - n : 0;
    if (!s->minus) emit_rep(o, ' ', pad);
    emit(o, str, n);
    if (s->minus) emit_rep(o, ' ', pad);
}

static void emit_integer(out_t *o, const spec_t *s, unsigned long long v,
                         int neg, int is_signed, unsigned base, int upper,
                         int force_prefix)
{
    char buf[24];                                   /* >= 22 octal digits for 64 bits */
    char *end = buf + sizeof buf, *q = end;
    const char *alpha = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char pre[2];
    size_t npre = 0, nd, zeros = 0, len, w = (size_t)s->width, pad;
    int nonzero = (v != 0);

    if (!(v == 0 && s->prec == 0))
        do { *--q = alpha[v % base]; v /= base; } while (v);
    nd = (size_t)(end - q);

    if (is_signed) {
        if (neg) pre[npre++] = '-';
        else if (s->plus) pre[npre++] = '+';
        else if (s->space) pre[npre++] = ' ';
    }
    if (base == 16 && (force_prefix || (s->alt && nonzero))) {
        pre[0] = '0'; pre[1] = upper ? 'X' : 'x'; npre = 2;   /* never combined with a sign */
    }
    if (s->prec > 0 && (size_t)s->prec > nd) zeros = (size_t)s->prec - nd;
    if (base == 8 && s->alt && zeros == 0 && (nd == 0 || *q != '0')) zeros = 1;

    len = npre + zeros + nd;
    pad = w > len ? w - len : 0;

    if (s->minus) {
        emit(o, pre, npre); emit_rep(o, '0', zeros); emit(o, q, nd);
        emit_rep(o, ' ', pad);
    } else if (s->zero && s->prec < 0) {
        emit(o, pre, npre); emit_rep(o, '0', zeros + pad); emit(o, q, nd);
    } else {
        emit_rep(o, ' ', pad);
        emit(o, pre, npre); emit_rep(o, '0', zeros); emit(o, q, nd);
    }
}

/* Parses a decimal number; returns -1 on int overflow. */
static int parse_num(const char **pp)
{
    const char *p = *pp;
    int v = 0;
    while (isdigit((unsigned char)*p)) {
        int d = *p++ - '0';
        if (v > (INT_MAX - d) / 10) return -1;
        v = v * 10 + d;
    }
    *pp = p;
    return v;
}


/* ---- floating point ---------------------------------------------------- */

/* A rounded decimal: digit i (0-based) of the value, which is
 * 0.D0 D1 D2 ... * 10^dp; digits past `n` are zero. */
typedef struct { char d[NOVAC_DTOA_MAX + 2]; int n, dp; } dec_t;

static char dig_at(const dec_t *r, long i) { return (i >= 0 && i < r->n) ? r->d[i] : '0'; }

/* Rounds to `keep` significant digits (ties to even). keep may be <= 0, in
 * which case the result is 0 or a single 1 one place up. */
static void dec_round(dec_t *r, long keep)
{
    int up;
    if (keep >= r->n) return;
    if (keep < 0) { r->n = 0; return; }
    if (r->d[keep] > '5') up = 1;
    else if (r->d[keep] < '5') up = 0;
    else if (r->n > keep + 1) up = 1;                       /* digits after the 5 are nonzero */
    else up = keep > 0 ? ((r->d[keep - 1] - '0') & 1) : 0;  /* exact tie: to even */
    r->n = (int)keep;
    if (up) {
        long i = keep - 1;
        while (i >= 0 && r->d[i] == '9') i--;
        if (i < 0) { r->d[0] = '1'; r->n = 1; r->dp++; }
        else { r->d[i]++; r->n = (int)i + 1; }
    }
    while (r->n > 0 && r->d[r->n - 1] == '0') r->n--;
}

static void emit_digits(out_t *o, const dec_t *r, long from, long count)
{
    char buf[32];
    while (count > 0 && !o->overflow) {
        long k = count < (long)sizeof buf ? count : (long)sizeof buf, i;
        if (from >= r->n || from + k <= 0) {     /* all zeros: no need to look */
            emit_rep(o, '0', (size_t)k);
        } else {
            for (i = 0; i < k; i++) buf[i] = dig_at(r, from + i);
            emit(o, buf, (size_t)k);
        }
        from += k; count -= k;
    }
}

static void pad_before(out_t *o, const spec_t *s, size_t len, int zero_ok, const char *pre, size_t npre)
{
    size_t w = (size_t)s->width, pad = w > len ? w - len : 0;
    if (s->minus) { emit(o, pre, npre); return; }
    if (s->zero && zero_ok) { emit(o, pre, npre); emit_rep(o, '0', pad); return; }
    emit_rep(o, ' ', pad);
    emit(o, pre, npre);
}
static void pad_after(out_t *o, const spec_t *s, size_t len)
{
    size_t w = (size_t)s->width;
    if (s->minus && w > len) emit_rep(o, ' ', w - len);
}

static void emit_hexfloat(out_t *o, const spec_t *s, uint64_t bits, int upper, const char *sign, size_t nsign)
{
    const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    uint64_t mant = bits & ((1ULL << 52) - 1);
    int bexp = (int)((bits >> 52) & 0x7ff), e, lead, nd, i;
    char pre[4], frac[13], expbuf[8];
    size_t npre = 0, nexp = 0, len;
    long prec = s->prec;
    unsigned ue;

    if (bexp == 0 && mant == 0) { lead = 0; e = 0; }
    else if (bexp == 0) { lead = 0; e = -1022; }
    else { lead = 1; e = bexp - 1023; }

    if (prec >= 0 && prec < 13) {                /* round to prec hex digits, ties to even */
        int drop = (int)(13 - prec) * 4;
        uint64_t rem = mant & ((1ULL << drop) - 1), half = 1ULL << (drop - 1);
        uint64_t keep = mant >> drop;
        if (rem > half || (rem == half && (prec > 0 ? (keep & 1) : (lead & 1)))) keep++;
        if (keep >> (prec * 4)) { lead++; keep = 0; }   /* carried into the leading digit */
        mant = keep << drop;
    }
    for (i = 0; i < 13; i++) frac[i] = hex[(mant >> (48 - 4 * i)) & 15];
    if (prec < 0) { nd = 13; while (nd > 0 && frac[nd - 1] == '0') nd--; }
    else nd = (int)(prec < 13 ? prec : 13);

    memcpy(pre, sign, nsign); npre = nsign;
    pre[npre++] = '0'; pre[npre++] = upper ? 'X' : 'x';
    expbuf[nexp++] = upper ? 'P' : 'p';
    expbuf[nexp++] = e < 0 ? '-' : '+';
    ue = (unsigned)(e < 0 ? -e : e);
    { char t[6]; int k = 0; do { t[k++] = (char)('0' + ue % 10); ue /= 10; } while (ue);
      while (k) expbuf[nexp++] = t[--k]; }

    {
        long fraclen = prec < 0 ? nd : prec;
        int point = fraclen > 0 || s->alt;
        char leadc = hex[lead & 15];
        len = npre + 1 + (size_t)point + (size_t)fraclen + nexp;
        pad_before(o, s, len, 1, pre, npre);
        emit(o, &leadc, 1);
        if (point) emit(o, ".", 1);
        emit(o, frac, (size_t)nd);
        if (fraclen > nd) emit_rep(o, '0', (size_t)(fraclen - nd));
        emit(o, expbuf, nexp);
        pad_after(o, s, len);
    }
}

static void emit_float(out_t *o, const spec_t *s, double v, char conv)
{
    dec_t r;
    uint64_t bits;
    int neg, upper = (conv == 'F' || conv == 'E' || conv == 'G' || conv == 'A');
    char sign[1];
    size_t nsign = 0, len;
    long prec = s->prec;
    char lc = (char)(conv | 0x20);

    memcpy(&bits, &v, sizeof bits);
    neg = (int)(bits >> 63);
    if (neg) sign[nsign++] = '-';
    else if (s->plus) sign[nsign++] = '+';
    else if (s->space) sign[nsign++] = ' ';

    if (((bits >> 52) & 0x7ff) == 0x7ff) {       /* inf / nan: never zero-padded */
        const char *w = (bits & ((1ULL << 52) - 1)) ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        len = nsign + 3;
        pad_before(o, s, len, 0, sign, nsign);
        emit(o, w, 3);
        pad_after(o, s, len);
        return;
    }
    if (lc == 'a') { emit_hexfloat(o, s, bits, upper, sign, nsign); return; }

    if (prec < 0) prec = 6;
    if ((bits << 1) == 0) { r.n = 0; r.dp = 1; }
    else r.n = novac__dtoa_exact(neg ? -v : v, r.d, &r.dp);

    if (lc == 'g') {
        long P = prec == 0 ? 1 : prec, X;
        dec_round(&r, P);
        X = r.n == 0 && (bits << 1) == 0 ? 0 : r.dp - 1;
        if (r.n == 0) X = 0;
        if (P > X && X >= -4) {
            lc = 'f';
            prec = P - 1 - X;
            if (!s->alt) { long sig = r.n - (X + 1); prec = sig > 0 ? sig : 0; }
        } else {
            lc = 'e';
            prec = P - 1;
            if (!s->alt) prec = r.n > 0 ? r.n - 1 : 0;
        }
    }

    if (lc == 'e') {
        long X;
        char expbuf[8];
        size_t nexp = 0;
        unsigned long ue;
        dec_round(&r, prec + 1);
        X = r.n == 0 ? 0 : r.dp - 1;
        expbuf[nexp++] = upper ? 'E' : 'e';
        expbuf[nexp++] = X < 0 ? '-' : '+';
        ue = (unsigned long)(X < 0 ? -X : X);
        { char t[8]; int k = 0; do { t[k++] = (char)('0' + ue % 10); ue /= 10; } while (ue);
          if (k < 2) t[k++] = '0';
          while (k) expbuf[nexp++] = t[--k]; }
        {
            int point = prec > 0 || s->alt;
            char first = dig_at(&r, 0);
            len = nsign + 1 + (size_t)point + (size_t)prec + nexp;
            pad_before(o, s, len, 1, sign, nsign);
            emit(o, &first, 1);
            if (point) emit(o, ".", 1);
            emit_digits(o, &r, 1, prec);
            emit(o, expbuf, nexp);
            pad_after(o, s, len);
        }
        return;
    }

    /* 'f' */
    {
        long intlen, point;
        dec_round(&r, r.dp + prec);
        if (r.n == 0) r.dp = 1;                  /* rounded to zero */
        intlen = r.dp > 0 ? r.dp : 1;
        point = prec > 0 || s->alt;
        len = nsign + (size_t)intlen + (size_t)point + (size_t)prec;
        pad_before(o, s, len, 1, sign, nsign);
        if (r.dp > 0) emit_digits(o, &r, 0, r.dp);
        else emit(o, "0", 1);
        if (point) emit(o, ".", 1);
        emit_digits(o, &r, r.dp, prec);
        pad_after(o, s, len);
    }
}

static int fail(int err) { errno = err; return -1; }

int novac_format(novac_sink_fn sink, void *ctx, const char *fmt, va_list ap)
{
    out_t o = { sink, ctx, 0, 0 };
    const char *p = fmt;

    if (!sink || !fmt) return fail(EINVAL);

    while (*p) {
        spec_t s = { 0, 0, 0, 0, 0, 0, -1, L_NONE };
        const char *lit = p;
        char conv;

        while (*p && *p != '%') p++;
        emit(&o, lit, (size_t)(p - lit));
        if (!*p) break;
        p++;

        for (;; p++) {
            if      (*p == '-') s.minus = 1;
            else if (*p == '+') s.plus = 1;
            else if (*p == ' ') s.space = 1;
            else if (*p == '#') s.alt = 1;
            else if (*p == '0') s.zero = 1;
            else break;
        }
        if (*p == '*') {
            int w = va_arg(ap, int);
            p++;
            if (w < 0) {
                if (w == INT_MIN) return fail(EOVERFLOW);
                s.minus = 1; w = -w;
            }
            s.width = w;
        } else if (isdigit((unsigned char)*p)) {
            s.width = parse_num(&p);
            if (s.width < 0) return fail(EOVERFLOW);
        }
        if (*p == '.') {
            p++;
            if (*p == '*') {
                int pr = va_arg(ap, int);
                p++;
                s.prec = pr < 0 ? -1 : pr;          /* negative precision = absent */
            } else {
                s.prec = parse_num(&p);
                if (s.prec < 0) return fail(EOVERFLOW);
            }
        }
        switch (*p) {
        case 'h': p++; if (*p == 'h') { s.len = L_HH; p++; } else s.len = L_H; break;
        case 'l': p++; if (*p == 'l') { s.len = L_LL; p++; } else s.len = L_L; break;
        case 'z': s.len = L_Z; p++; break;
        case 't': s.len = L_T; p++; break;
        case 'j': s.len = L_J; p++; break;
        case 'L': s.len = L_BIGL; p++; break;
        default: break;
        }

        conv = *p;
        if (!conv) return fail(EINVAL);             /* format ends after '%' */
        p++;

        switch (conv) {
        case '%':
            emit(&o, "%", 1);
            break;
        case 'c': {
            char ch;
            if (s.len != L_NONE) return fail(EINVAL);
            ch = (char)va_arg(ap, int);
            emit_padded(&o, &s, &ch, 1);
            break;
        }
        case 's': {
            const char *str;
            size_t n;
            if (s.len != L_NONE) return fail(EINVAL);
            str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            n = s.prec < 0 ? strlen(str) : strnlen(str, (size_t)s.prec);
            emit_padded(&o, &s, str, n);
            break;
        }
        case 'd': case 'i': {
            long long sv;
            if (s.len == L_BIGL) return fail(EINVAL);
            switch (s.len) {
            case L_HH: sv = (signed char)va_arg(ap, int); break;
            case L_H:  sv = (short)va_arg(ap, int); break;
            case L_L:  sv = va_arg(ap, long); break;
            case L_LL: sv = va_arg(ap, long long); break;
            case L_Z:
            case L_T:  sv = va_arg(ap, ptrdiff_t); break;
            case L_J:  sv = va_arg(ap, intmax_t); break;
            default:   sv = va_arg(ap, int); break;
            }
            emit_integer(&o, &s, sv < 0 ? 0ull - (unsigned long long)sv
                                        : (unsigned long long)sv,
                         sv < 0, 1, 10, 0, 0);
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            unsigned long long uv;
            if (s.len == L_BIGL) return fail(EINVAL);
            switch (s.len) {
            case L_HH: uv = (unsigned char)va_arg(ap, unsigned); break;
            case L_H:  uv = (unsigned short)va_arg(ap, unsigned); break;
            case L_L:  uv = va_arg(ap, unsigned long); break;
            case L_LL: uv = va_arg(ap, unsigned long long); break;
            case L_Z:
            case L_T:  uv = va_arg(ap, size_t); break;
            case L_J:  uv = va_arg(ap, uintmax_t); break;
            default:   uv = va_arg(ap, unsigned); break;
            }
            emit_integer(&o, &s, uv, 0, 0,
                         conv == 'o' ? 8u : (conv == 'u' ? 10u : 16u),
                         conv == 'X', 0);
            break;
        }
        case 'p': {
            spec_t ps = s;
            if (s.len != L_NONE) return fail(EINVAL);
            ps.zero = 0; ps.plus = 0; ps.space = 0;
            emit_integer(&o, &ps, (uintptr_t)va_arg(ap, void *), 0, 0, 16, 0, 1);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            double v;
            if (s.len == L_BIGL) v = (double)va_arg(ap, long double);
            else if (s.len == L_NONE || s.len == L_L) v = va_arg(ap, double);
            else return fail(EINVAL);
            emit_float(&o, &s, v, conv);
            break;
        }
        default:
            return fail(EINVAL);                    /* %n, unknown */
        }
        if (o.overflow) return fail(EOVERFLOW);
    }
    return (int)o.total;
}
