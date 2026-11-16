/* The scanf engine, and sscanf/vsscanf over it.
 *
 * Conversions: d i u o x X p n c s [ f F e E g G a A %; length modifiers
 * hh h l ll j z t L q; '*' suppression; field widths; and POSIX's 'm'
 * (allocate the string) for c, s and [. A number is read as the longest
 * prefix of a valid number, as C requires, so "100ergs" fails %f: with
 * one character of push-back, the "e" is already gone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <novac/scan.h>

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL };

typedef struct {
    const novac_scan_src *src;
    long nread;
    int eof;
} in_t;

static int in_get(in_t *in)
{
    int c = in->src->get(in->src->ctx);
    if (c < 0) { in->eof = 1; return -1; }
    in->nread++;
    return c;
}
static void in_unget(in_t *in, int c)
{
    if (c < 0) return;
    in->src->unget(c, in->src->ctx);
    in->nread--;
}
static void skip_space(in_t *in)
{
    int c;
    while ((c = in_get(in)) >= 0 && isspace(c)) {}
    in_unget(in, c);
}

/* A growable text buffer for a numeric field. */
typedef struct { char small[128]; char *p; size_t len, cap; int oom; } tbuf;
static void tb_init(tbuf *t) { t->p = t->small; t->len = 0; t->cap = sizeof t->small; t->oom = 0; t->p[0] = 0; }
static void tb_put(tbuf *t, int c)
{
    if (t->len + 2 > t->cap) {
        size_t nc = t->cap * 2;
        char *np = t->p == t->small ? malloc(nc) : realloc(t->p, nc);
        if (!np) { t->oom = 1; return; }
        if (t->p == t->small) memcpy(np, t->small, t->len);
        t->p = np; t->cap = nc;
    }
    t->p[t->len++] = (char)c;
    t->p[t->len] = 0;
}
static void tb_free(tbuf *t) { if (t->p != t->small) free(t->p); }

/* Width-limited reader: get() that stops after `left` characters. */
typedef struct { in_t *in; long left; } field_t;
static int f_get(field_t *f)
{
    if (f->left == 0) return -1;
    { int c = in_get(f->in); if (c >= 0) f->left--; return c; }
}
static void f_unget(field_t *f, int c) { if (c >= 0) { in_unget(f->in, c); f->left++; } }

static int digit_ok(int c, int base)
{
    if (base <= 10) return c >= '0' && c < '0' + base;
    return isdigit(c) || (tolower(c) >= 'a' && tolower(c) < 'a' + base - 10);
}

/* Reads the longest prefix of an integer in `base` (0 = C rules). */
static void read_int(field_t *f, tbuf *t, int *base)
{
    int c = f_get(f);
    if (c == '+' || c == '-') { tb_put(t, c); c = f_get(f); }
    if (c == '0' && (*base == 0 || *base == 16)) {
        tb_put(t, c);
        c = f_get(f);
        if (c == 'x' || c == 'X') { tb_put(t, c); *base = 16; c = f_get(f); }
        else if (*base == 0) *base = 8;
    } else if (*base == 0) *base = 10;
    while (c >= 0 && digit_ok(c, *base)) { tb_put(t, c); c = f_get(f); }
    f_unget(f, c);
}

static int lower(int c) { return tolower(c); }

/* Reads the longest prefix of a floating-point number. */
static void read_float(field_t *f, tbuf *t)
{
    int c = f_get(f), any = 0, hex = 0, base;
    if (c == '+' || c == '-') { tb_put(t, c); c = f_get(f); }
    if (lower(c) == 'i' || lower(c) == 'n') {
        const char *word = lower(c) == 'i' ? "infinity" : "nan";
        size_t i = 0;
        while (word[i] && lower(c) == word[i]) { tb_put(t, c); i++; c = f_get(f); }
        if (word[0] == 'i' && i > 3 && word[i]) {}     /* "infin": invalid, left in buffer */
        if (word[0] == 'n' && i == 3 && c == '(') {
            tb_put(t, c);
            c = f_get(f);
            while (c >= 0 && (isalnum(c) || c == '_')) { tb_put(t, c); c = f_get(f); }
            if (c == ')') { tb_put(t, c); c = f_get(f); }
        }
        f_unget(f, c);
        return;
    }
    if (c == '0') {
        tb_put(t, c); any = 1;
        c = f_get(f);
        if (c == 'x' || c == 'X') { tb_put(t, c); hex = 1; any = 0; c = f_get(f); }
    }
    base = hex ? 16 : 10;
    while (c >= 0 && digit_ok(c, base)) { tb_put(t, c); any = 1; c = f_get(f); }
    if (c == '.') {
        tb_put(t, c);
        c = f_get(f);
        while (c >= 0 && digit_ok(c, base)) { tb_put(t, c); any = 1; c = f_get(f); }
    }
    if (any && lower(c) == (hex ? 'p' : 'e')) {
        tb_put(t, c);
        c = f_get(f);
        if (c == '+' || c == '-') { tb_put(t, c); c = f_get(f); }
        while (c >= 0 && isdigit(c)) { tb_put(t, c); c = f_get(f); }
    }
    f_unget(f, c);
}

static void store_int(void *p, int len, unsigned long long v)
{
    switch (len) {
    case L_HH: *(signed char *)p = (signed char)v; break;
    case L_H:  *(short *)p = (short)v; break;
    case L_L:  *(long *)p = (long)v; break;
    case L_LL: case L_BIGL: *(long long *)p = (long long)v; break;
    case L_J:  *(intmax_t *)p = (intmax_t)v; break;
    case L_Z:  *(size_t *)p = (size_t)v; break;
    case L_T:  *(ptrdiff_t *)p = (ptrdiff_t)v; break;
    default:   *(int *)p = (int)v; break;
    }
}

/* Parses a scanset after '['; returns the format position after ']'. */
static const char *parse_set(const char *f, unsigned char set[256])
{
    int invert = 0, c;
    memset(set, 0, 256);
    if (*f == '^') { invert = 1; f++; }
    if (*f == ']') { set[']'] = 1; f++; }
    while (*f && *f != ']') {
        if (f[1] == '-' && f[2] && f[2] != ']' && (unsigned char)f[2] >= (unsigned char)f[0]) {
            for (c = (unsigned char)f[0]; c <= (unsigned char)f[2]; c++) set[c] = 1;
            f += 3;
        } else set[(unsigned char)*f++] = 1;
    }
    if (*f == ']') f++;
    if (invert) for (c = 0; c < 256; c++) set[c] = !set[c];
    return f;
}

int novac_scan(const novac_scan_src *src, const char *fmt, va_list ap)
{
    in_t in = { src, 0, 0 };
    const unsigned char *f = (const unsigned char *)fmt;
    int assigned = 0, converted = 0, input_failure = 0;

    while (*f) {
        int suppress = 0, len = L_NONE, alloc = 0, c;
        long width = -1;
        unsigned char conv;

        if (isspace(*f)) { while (isspace(*f)) f++; skip_space(&in); continue; }
        if (*f != '%' || f[1] == '%') {
            int want = *f;
            if (*f == '%') { f++; skip_space(&in); }
            f++;
            c = in_get(&in);
            if (c != want) { in_unget(&in, c); if (c < 0) input_failure = 1; break; }
            continue;
        }
        f++;
        if (*f == '*') { suppress = 1; f++; }
        if (isdigit(*f)) { width = 0; while (isdigit(*f)) { if (width < 100000000) width = width * 10 + (*f - '0'); f++; } }
        if (*f == 'm') { alloc = 1; f++; }
        switch (*f) {
        case 'h': f++; if (*f == 'h') { len = L_HH; f++; } else len = L_H; break;
        case 'l': f++; if (*f == 'l') { len = L_LL; f++; } else len = L_L; break;
        case 'q': f++; len = L_LL; break;
        case 'j': f++; len = L_J; break;
        case 'z': f++; len = L_Z; break;
        case 't': f++; len = L_T; break;
        case 'L': f++; len = L_BIGL; break;
        default: break;
        }
        conv = *f++;
        if (!conv) break;
        if (width == 0) break;                       /* "%0d" is invalid */
        if (conv != '[' && conv != 'c' && conv != 'n') skip_space(&in);

        switch (conv) {
        case 'n':
            if (!suppress) store_int(va_arg(ap, void *), len, (unsigned long long)in.nread);
            continue;

        case 'c': case 's': case '[': {
            unsigned char set[256];
            char *out = NULL, **outp = NULL;
            size_t n = 0, cap = 0;
            long max = width;
            if (len == L_L) goto done;               /* wide characters: unsupported */
            if (conv == '[') f = (const unsigned char *)parse_set((const char *)f, set);
            if (conv == 'c' && max < 0) max = 1;
            if (!suppress) {
                if (alloc) {
                    outp = va_arg(ap, char **);
                    cap = conv == 'c' ? (size_t)max : 16;
                    out = malloc(cap + 1);
                    if (!out) goto done;
                } else out = va_arg(ap, char *);
            }
            while (max < 0 || (long)n < max) {
                c = in_get(&in);
                if (c < 0) break;
                if ((conv == 's' && isspace(c)) || (conv == '[' && !set[c])) { in_unget(&in, c); break; }
                if (out) {
                    if (alloc && n + 1 >= cap + 1) {
                        char *np = realloc(out, cap * 2 + 1);
                        if (!np) { free(out); goto done; }
                        out = np; cap *= 2;
                    }
                    out[n] = (char)c;
                }
                n++;
            }
            if (n == 0 || (conv == 'c' && (long)n < max)) {
                if (alloc && out) free(out);
                if (in.eof && n == 0) input_failure = 1;
                if (conv == 'c' && n > 0) {}          /* short %c: a matching failure */
                goto done;
            }
            if (out && conv != 'c') out[n] = 0;
            if (outp) *outp = out;
            if (!suppress) assigned++;
            converted++;
            continue;
        }

        case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'p': {
            tbuf t;
            field_t fl = { &in, width };
            int base = conv == 'd' || conv == 'u' ? 10 : conv == 'o' ? 8 :
                       conv == 'i' ? 0 : 16;
            char *end;
            unsigned long long v;
            tb_init(&t);
            read_int(&fl, &t, &base);
            if (t.oom) { tb_free(&t); goto done; }
            {
                int save = errno;
                if (conv == 'd' || conv == 'i') v = (unsigned long long)strtoll(t.p, &end, base);
                else v = strtoull(t.p, &end, base);
                errno = save;
            }
            /* "0x" with no hex digits after it still converts the 0 */
            if (end == t.p || (*end && !(base == 16 && end[0] && (end[0] == 'x' || end[0] == 'X') && !end[1]))) {
                if (t.len == 0 && in.eof) input_failure = 1;
                tb_free(&t);
                goto done;
            }
            tb_free(&t);
            if (!suppress) {
                if (conv == 'p') *va_arg(ap, void **) = (void *)(uintptr_t)v;
                else store_int(va_arg(ap, void *), len, v);
                assigned++;
            }
            converted++;
            continue;
        }

        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            tbuf t;
            field_t fl = { &in, width };
            char *end;
            tb_init(&t);
            read_float(&fl, &t);
            if (t.oom) { tb_free(&t); goto done; }
            {
                int save = errno;
                if (len == L_NONE) {
                    float v = strtof(t.p, &end);
                    if (end == t.p + t.len && t.len && !suppress) *va_arg(ap, float *) = v;
                } else {
                    double v = strtod(t.p, &end);
                    if (end == t.p + t.len && t.len && !suppress) {
                        if (len == L_BIGL) *va_arg(ap, long double *) = v;
                        else *va_arg(ap, double *) = v;
                    }
                }
                errno = save;
            }
            if (t.len == 0 || end != t.p + t.len) {
                if (t.len == 0 && in.eof) input_failure = 1;
                tb_free(&t);
                goto done;
            }
            tb_free(&t);
            if (!suppress) assigned++;
            converted++;
            continue;
        }

        default:
            goto done;                               /* unknown conversion */
        }
    }
done:
    if (input_failure && converted == 0) return EOF;
    return assigned;
}

static int str_get(void *ctx)
{
    const unsigned char **p = ctx;
    if (!**p) return -1;
    return *(*p)++;
}
static void str_unget(int c, void *ctx)
{
    const unsigned char **p = ctx;
    (void)c;
    (*p)--;
}

int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap)
{
    const unsigned char *p = (const unsigned char *)s;
    novac_scan_src src = { str_get, str_unget, &p };
    return novac_scan(&src, fmt, ap);
}

int sscanf(const char *restrict s, const char *restrict fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}
