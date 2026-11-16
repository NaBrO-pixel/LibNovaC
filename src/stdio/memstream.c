/* fmemopen and open_memstream, as fopencookie backends. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ---- fmemopen ---------------------------------------------------------- *
 * POSIX semantics: the stream's size is `size`; "end" is where the content
 * stops (0 for "w", the first NUL for "a", `size` for "r"); writes past
 * `size` fail with ENOSPC; after a write, a NUL is stored at the end of the
 * content if there is room. */
typedef struct { char *buf; size_t size, pos, end; int append, own; } mem_t;

static ssize_t mem_read(void *c, char *out, size_t n)
{
    mem_t *m = c;
    size_t k;
    if (m->pos >= m->end) return 0;
    k = m->end - m->pos;
    if (k > n) k = n;
    memcpy(out, m->buf + m->pos, k);
    m->pos += k;
    return (ssize_t)k;
}

static ssize_t mem_write(void *c, const char *in, size_t n)
{
    mem_t *m = c;
    size_t k;
    if (m->append) m->pos = m->end;
    if (m->pos >= m->size) { errno = ENOSPC; return 0; }
    k = m->size - m->pos;
    if (k > n) k = n;
    memcpy(m->buf + m->pos, in, k);
    m->pos += k;
    if (m->pos > m->end) m->end = m->pos;
    if (m->end < m->size) m->buf[m->end] = 0;
    if (k < n) { errno = ENOSPC; }
    return (ssize_t)k;
}

static int mem_seek(void *c, off_t *off, int whence)
{
    mem_t *m = c;
    long long base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (long long)m->pos :
                     whence == SEEK_END ? (long long)m->end : -1;
    long long to;
    if (base < 0) { errno = EINVAL; return -1; }
    to = base + *off;
    if (to < 0 || to > (long long)m->size) { errno = EINVAL; return -1; }
    m->pos = (size_t)to;
    *off = to;
    return 0;
}

static int mem_close(void *c)
{
    mem_t *m = c;
    if (m->own) free(m->buf);
    free(m);
    return 0;
}

FILE *fmemopen(void *restrict buf, size_t size, const char *restrict mode)
{
    static const cookie_io_functions_t io = { mem_read, mem_write, mem_seek, mem_close };
    mem_t *m;
    FILE *f;
    int plus = mode && strchr(mode, '+') != NULL;
    if (!mode || !strchr("rwa", mode[0]) || size == 0) { errno = EINVAL; return NULL; }
    if (!buf && !plus) { errno = EINVAL; return NULL; }   /* a buffer nobody could read back */
    m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->size = size;
    if (buf) m->buf = buf;
    else {
        m->buf = calloc(1, size);
        if (!m->buf) { free(m); return NULL; }
        m->own = 1;
    }
    switch (mode[0]) {
    case 'r': m->end = size; break;
    case 'w': m->end = 0; m->buf[0] = 0; break;
    default:  m->end = strnlen(m->buf, size); m->pos = m->end; m->append = 1; break;
    }
    if (!buf && mode[0] == 'r') m->end = 0;
    f = fopencookie(m, mode, io);
    if (!f) { mem_close(m); return NULL; }
    return f;
}

/* ---- open_memstream ------------------------------------------------------ */
typedef struct { char **bufp; size_t *sizep; char *buf; size_t cap, len, pos; } ms_t;

static void ms_publish(ms_t *s)
{
    *s->bufp = s->buf;
    *s->sizep = s->pos < s->len ? s->pos : s->len;
}

static ssize_t ms_write(void *c, const char *in, size_t n)
{
    ms_t *s = c;
    if (s->pos + n + 1 > s->cap) {
        size_t nc = s->cap;
        char *p;
        while (s->pos + n + 1 > nc) nc *= 2;
        p = realloc(s->buf, nc);
        if (!p) return 0;
        memset(p + s->cap, 0, nc - s->cap);
        s->buf = p; s->cap = nc;
    }
    memcpy(s->buf + s->pos, in, n);
    s->pos += n;
    if (s->pos > s->len) s->len = s->pos;
    s->buf[s->len] = 0;
    ms_publish(s);
    return (ssize_t)n;
}

static int ms_seek(void *c, off_t *off, int whence)
{
    ms_t *s = c;
    long long base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (long long)s->pos :
                     whence == SEEK_END ? (long long)s->len : -1, to;
    if (base < 0) { errno = EINVAL; return -1; }
    to = base + *off;
    if (to < 0) { errno = EINVAL; return -1; }
    if ((size_t)to + 1 > s->cap) {           /* seeking past the end extends with zeros */
        size_t nc = s->cap;
        char *p;
        while ((size_t)to + 1 > nc) nc *= 2;
        p = realloc(s->buf, nc);
        if (!p) { errno = ENOMEM; return -1; }
        memset(p + s->cap, 0, nc - s->cap);
        s->buf = p; s->cap = nc;
    }
    s->pos = (size_t)to;
    ms_publish(s);
    *off = to;
    return 0;
}

static int ms_close(void *c)
{
    ms_t *s = c;
    ms_publish(s);
    free(s);
    return 0;
}

FILE *open_memstream(char **bufp, size_t *sizep)
{
    static const cookie_io_functions_t io = { NULL, ms_write, ms_seek, ms_close };
    ms_t *s;
    FILE *f;
    if (!bufp || !sizep) { errno = EINVAL; return NULL; }
    s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->cap = 64;
    s->buf = calloc(1, s->cap);
    if (!s->buf) { free(s); return NULL; }
    s->bufp = bufp; s->sizep = sizep;
    ms_publish(s);
    f = fopencookie(s, "w", io);
    if (!f) { free(s->buf); free(s); return NULL; }
    return f;
}
