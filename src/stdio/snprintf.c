#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <novac/format.h>

typedef struct { char *buf; size_t cap; size_t pos; } bufctx_t;

/* Stores as much as fits while always leaving room for the terminator. */
static void buf_sink(void *c, const char *s, size_t n)
{
    bufctx_t *b = c;
    size_t room;
    if (b->cap == 0 || b->pos >= b->cap - 1) return;
    room = b->cap - 1 - b->pos;
    if (n > room) n = room;
    memcpy(b->buf + b->pos, s, n);
    b->pos += n;
}

int vsnprintf(char *restrict buf, size_t n, const char *restrict fmt, va_list ap)
{
    bufctx_t c = { buf, n, 0 };
    int r;
    if (n > 0 && !buf) { errno = EINVAL; return -1; }
    r = novac_format(buf_sink, &c, fmt, ap);
    if (n > 0) buf[c.pos] = '\0';                   /* pos <= n-1 by construction */
    return r;
}

int vsprintf(char *restrict buf, const char *restrict fmt, va_list ap)
{
    return vsnprintf(buf, (size_t)INT_MAX, fmt, ap);
}

int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *restrict buf, const char *restrict fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

/* asprintf: formats into a buffer that grows as needed. */
#include <stdlib.h>
typedef struct { char *buf; size_t cap, len; int oom; } growctx_t;
static void grow_sink(void *v, const char *s, size_t n)
{
    growctx_t *g = v;
    if (g->oom) return;
    if (g->len + n + 1 > g->cap) {
        size_t nc = g->cap ? g->cap : 64;
        char *p;
        while (g->len + n + 1 > nc) nc *= 2;
        p = realloc(g->buf, nc);
        if (!p) { g->oom = 1; return; }
        g->buf = p; g->cap = nc;
    }
    memcpy(g->buf + g->len, s, n);
    g->len += n;
}

int vasprintf(char **restrict out, const char *restrict fmt, va_list ap)
{
    growctx_t g = { NULL, 0, 0, 0 };
    int r = novac_format(grow_sink, &g, fmt, ap);
    if (r < 0 || g.oom) { free(g.buf); if (g.oom) errno = ENOMEM; return -1; }
    if (!g.buf) { g.buf = malloc(1); if (!g.buf) { errno = ENOMEM; return -1; } }
    g.buf[g.len] = 0;
    *out = g.buf;
    return r;
}

int asprintf(char **restrict out, const char *restrict fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}
