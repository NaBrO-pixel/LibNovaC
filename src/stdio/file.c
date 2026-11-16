/* Buffered FILE streams.
 *
 * A stream reads and writes through a backend: a file descriptor by
 * default, or the four functions of fopencookie(), which is how fmemopen
 * and open_memstream are built. One buffer per stream serves either reading
 * or writing, never both at once; switching direction flushes or gives back
 * unread input first. stdout is line buffered, stderr unbuffered, stdin
 * fully buffered; reading a stream that has to wait for more input flushes
 * stdout first, so prompts appear. Not thread-safe (the *_unlocked forms
 * and flockfile exist for source compatibility). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <novac/format.h>
#include <novac/scan.h>
#include "../internal/exit_hook.h"

enum { F_READ = 1, F_WRITE = 2, F_EOF = 4, F_ERR = 8, F_OWNBUF = 16, F_STATIC = 32, F_APPEND = 64 };

struct novac_file {
    int fd, flags, bufmode, ungot;
    unsigned char *buf;
    size_t bufsize, rpos, rend, wlen;
    unsigned char tiny[1];
    struct novac_file *next;
    void *cookie;                       /* fopencookie backend, if io.read etc. are set */
    cookie_io_functions_t io;
    pid_t pid;                          /* popen's child */
};

static unsigned char in_buf[BUFSIZ], out_buf[BUFSIZ];
static struct novac_file f_err = { 2, F_WRITE | F_STATIC, _IONBF, -1, NULL, 0, 0, 0, 0, {0}, NULL, NULL, {0}, 0 };
static struct novac_file f_out = { 1, F_WRITE | F_STATIC, _IOLBF, -1, out_buf, BUFSIZ, 0, 0, 0, {0}, &f_err, NULL, {0}, 0 };
static struct novac_file f_in  = { 0, F_READ  | F_STATIC, _IOFBF, -1, in_buf, BUFSIZ, 0, 0, 0, {0}, &f_out, NULL, {0}, 0 };

FILE *novac_stdin = &f_in, *novac_stdout = &f_out, *novac_stderr = &f_err;
static FILE *open_list = &f_in;
static int exit_hooked;

static void flush_all_at_exit(void) { fflush(NULL); }
static void hook_exit(void)
{
    if (!exit_hooked) { exit_hooked = 1; novac__set_exit_flush(flush_all_at_exit); }
}

/* ---- the backend --------------------------------------------------------- */

static int is_cookie(const FILE *f) { return f->io.read || f->io.write || f->io.seek || f->io.close; }

static ssize_t be_read(FILE *f, void *buf, size_t n)
{
    if (is_cookie(f)) {
        if (!f->io.read) { errno = EBADF; return -1; }
        return f->io.read(f->cookie, buf, n);
    }
    return read(f->fd, buf, n);
}

static ssize_t be_write(FILE *f, const void *buf, size_t n)
{
    if (is_cookie(f)) {
        ssize_t r;
        if (!f->io.write) { errno = EBADF; return -1; }
        r = f->io.write(f->cookie, buf, n);
        return r == 0 && n ? -1 : r;          /* glibc's convention: 0 is an error */
    }
    return write(f->fd, buf, n);
}

static off_t be_seek(FILE *f, off_t off, int whence)
{
    if (is_cookie(f)) {
        if (!f->io.seek) { errno = ESPIPE; return -1; }
        if (f->io.seek(f->cookie, &off, whence) != 0) return -1;
        return off;
    }
    return lseek(f->fd, off, whence);
}

static int be_close(FILE *f)
{
    if (is_cookie(f)) return f->io.close ? f->io.close(f->cookie) : 0;
    return f->fd >= 0 ? close(f->fd) : 0;
}

/* ---- buffering ------------------------------------------------------------ */

static int flush_write(FILE *f)
{
    size_t off = 0;
    while (off < f->wlen) {
        ssize_t w = be_write(f, f->buf + off, f->wlen - off);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) {
            if (w == 0) errno = EIO;
            f->flags |= F_ERR;
            f->wlen = 0;                       /* data is lost; the error flag says so */
            return -1;
        }
        off += (size_t)w;
    }
    f->wlen = 0;
    return 0;
}

/* Give back buffered-but-unconsumed input so the backend's offset equals
 * the stream's. */
static void drop_read(FILE *f)
{
    size_t unread = (f->rend - f->rpos) + (f->ungot >= 0 ? 1u : 0u);
    if (unread) {
        int saved = errno;
        be_seek(f, -(off_t)unread, SEEK_CUR);  /* fails harmlessly on pipes */
        errno = saved;
    }
    f->rpos = f->rend = 0;
    f->ungot = -1;
}

static int ensure_buf(FILE *f)
{
    if (f->buf) return 0;
    if (f->bufmode != _IONBF) {
        f->buf = malloc(BUFSIZ);
        if (f->buf) { f->bufsize = BUFSIZ; f->flags |= F_OWNBUF; return 0; }
    }
    f->buf = f->tiny; f->bufsize = 1; f->bufmode = _IONBF;   /* degrade, don't fail */
    return 0;
}

static int fill(FILE *f)
{
    ssize_t n;
    if (f->wlen && flush_write(f) != 0) return -1;
    if ((f == stdin || f->bufmode != _IOFBF) && f != stdout && stdout->wlen)
        flush_write(stdout);                    /* show a prompt before waiting for input */
    ensure_buf(f);
    do n = be_read(f, f->buf, f->bufsize); while (n < 0 && errno == EINTR);
    if (n < 0) { f->flags |= F_ERR; return -1; }
    if (n == 0) { f->flags |= F_EOF; return -1; }
    f->rpos = 0; f->rend = (size_t)n;
    return 0;
}

static int write_all(FILE *f, const unsigned char *p, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = be_write(f, p + off, n - off);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) { if (w == 0) errno = EIO; f->flags |= F_ERR; return -1; }
        off += (size_t)w;
    }
    return 0;
}

static int put_bytes(FILE *f, const void *data, size_t n)
{
    const unsigned char *p = data;
    size_t left = n;
    hook_exit();
    if (!(f->flags & F_WRITE)) { errno = EBADF; f->flags |= F_ERR; return -1; }
    if (f->rend > f->rpos || f->ungot >= 0) drop_read(f);
    ensure_buf(f);
    while (left) {
        size_t room = f->bufsize - f->wlen;
        if (room == 0) { if (flush_write(f) != 0) return -1; room = f->bufsize; }
        if (f->wlen == 0 && left >= f->bufsize && f->bufsize > 1)   /* big write: skip the copy */
            return write_all(f, p, left);
        { size_t k = left < room ? left : room;
          memcpy(f->buf + f->wlen, p, k);
          f->wlen += k; p += k; left -= k; }
    }
    if (f->bufmode == _IONBF || (f->bufmode == _IOLBF && memchr(data, '\n', n)))
        return flush_write(f);
    return 0;
}

/* ---- opening and closing ------------------------------------------------ */

/* Parses an fopen mode: open(2) flags in *oflags, stream flags returned
 * (0 = invalid). 'x' (exclusive) and 'e' (close-on-exec) are accepted. */
static int parse_mode(const char *mode, int *oflags)
{
    int fl, sf;
    switch (mode[0]) {
    case 'r': fl = O_RDONLY; sf = F_READ; break;
    case 'w': fl = O_WRONLY | O_CREAT | O_TRUNC; sf = F_WRITE; break;
    case 'a': fl = O_WRONLY | O_CREAT | O_APPEND; sf = F_WRITE | F_APPEND; break;
    default: return 0;
    }
    for (mode++; *mode; mode++) {
        if (*mode == '+') { fl = (fl & ~O_ACCMODE) | O_RDWR; sf |= F_READ | F_WRITE; }
        else if (*mode == 'x') fl |= O_EXCL;
        else if (*mode == 'e') fl |= O_CLOEXEC;
        else if (*mode != 'b') return 0;
    }
    *oflags = fl;
    return sf;
}

static FILE *new_stream(int fd, int sflags)
{
    FILE *f = calloc(1, sizeof *f);
    if (!f) return NULL;                       /* errno = ENOMEM from calloc */
    f->fd = fd;
    f->flags = sflags & (F_READ | F_WRITE | F_APPEND);
    f->bufmode = _IOFBF;
    f->ungot = -1;
    f->next = open_list;
    open_list = f;
    return f;
}

FILE *fopen(const char *restrict path, const char *restrict mode)
{
    int oflags, sflags, fd;
    FILE *f;
    if (!path || !mode || !(sflags = parse_mode(mode, &oflags))) { errno = EINVAL; return NULL; }
    fd = open(path, oflags, 0666);
    if (fd < 0) return NULL;
    f = new_stream(fd, sflags);
    if (!f) { int e = errno; close(fd); errno = e; }
    return f;
}

FILE *fdopen(int fd, const char *mode)
{
    int oflags, sflags;
    if (!mode || !(sflags = parse_mode(mode, &oflags))) { errno = EINVAL; return NULL; }
    if (fcntl(fd, F_GETFL) < 0) return NULL;   /* EBADF */
    return new_stream(fd, sflags);
}

FILE *fopencookie(void *cookie, const char *mode, cookie_io_functions_t io)
{
    int oflags, sflags;
    FILE *f;
    if (!mode || !(sflags = parse_mode(mode, &oflags))) { errno = EINVAL; return NULL; }
    f = new_stream(-1, sflags);
    if (!f) return NULL;
    f->cookie = cookie;
    f->io = io;
    if (!io.read && !io.write && !io.seek && !io.close) f->io.close = NULL;
    return f;
}

static void unlink_stream(FILE *f)
{
    FILE **pp;
    for (pp = &open_list; *pp && *pp != f; pp = &(*pp)->next) {}
    if (*pp) *pp = f->next;
}

int fflush(FILE *f)
{
    int rc = 0;
    if (!f) {
        for (FILE *p = open_list; p; p = p->next)
            if (p->wlen && flush_write(p) != 0) rc = EOF;
        return rc;
    }
    if (f->wlen && flush_write(f) != 0) return EOF;
    if (is_cookie(f) && f->io.seek && (f->flags & F_WRITE)) {
        off_t zero = 0;                      /* lets open_memstream publish its size */
        f->io.seek(f->cookie, &zero, SEEK_CUR);
    }
    return 0;
}

static int close_stream(FILE *f, int keep_object)
{
    int rc = 0;
    if (f->wlen && flush_write(f) != 0) rc = EOF;
    if (f->rend > f->rpos || f->ungot >= 0) drop_read(f);
    if (be_close(f) != 0) rc = EOF;
    if (f->flags & F_OWNBUF) free(f->buf);
    f->buf = NULL; f->bufsize = 0; f->flags &= ~F_OWNBUF;
    if (keep_object) return rc;
    unlink_stream(f);
    if (f->flags & F_STATIC) { f->fd = -1; f->flags = F_STATIC; f->next = NULL; }
    else free(f);
    return rc;
}

int fclose(FILE *f)
{
    if (!f) { errno = EINVAL; return EOF; }
    return close_stream(f, 0);
}

FILE *freopen(const char *restrict path, const char *restrict mode, FILE *restrict f)
{
    int oflags, sflags, fd;
    if (!path) { errno = EINVAL; close_stream(f, 0); return NULL; }   /* mode change only: unsupported */
    if (!mode || !(sflags = parse_mode(mode, &oflags))) { errno = EINVAL; return NULL; }
    close_stream(f, 1);
    fd = open(path, oflags, 0666);
    if (fd < 0) { int e = errno; unlink_stream(f); if (!(f->flags & F_STATIC)) free(f); errno = e; return NULL; }
    if (f->fd >= 0 && f->fd != fd && (f->flags & F_STATIC)) {      /* keep stdin/stdout/stderr's number */
        if (dup2(fd, f->fd) >= 0) { close(fd); fd = f->fd; }
    }
    f->fd = fd;
    f->flags = (f->flags & F_STATIC) | (sflags & (F_READ | F_WRITE | F_APPEND));
    f->ungot = -1; f->rpos = f->rend = f->wlen = 0;
    memset(&f->io, 0, sizeof f->io);
    f->cookie = NULL;
    if (f == stdout) { f->buf = out_buf; f->bufsize = BUFSIZ; }
    else if (f == stdin) { f->buf = in_buf; f->bufsize = BUFSIZ; }
    return f;
}

/* ---- reading ----------------------------------------------------------- */

static int read_ok(FILE *f)
{
    if (!(f->flags & F_READ)) { errno = EBADF; f->flags |= F_ERR; return 0; }
    if (f->wlen && flush_write(f) != 0) return 0;
    return 1;
}

int fgetc(FILE *f)
{
    if (f->ungot >= 0) { int c = f->ungot; f->ungot = -1; return c; }
    if (!read_ok(f)) return EOF;
    if (f->flags & F_EOF) return EOF;
    if (f->rpos == f->rend && fill(f) != 0) return EOF;
    return f->buf[f->rpos++];
}
int getc(FILE *f)  { return fgetc(f); }
int getchar(void)  { return fgetc(stdin); }

int ungetc(int c, FILE *f)
{
    if (c == EOF || f->ungot >= 0 || !(f->flags & F_READ)) return EOF;
    f->ungot = (unsigned char)c;
    f->flags &= ~F_EOF;
    return (unsigned char)c;
}

size_t fread(void *restrict buf, size_t size, size_t n, FILE *restrict f)
{
    unsigned char *p = buf;
    size_t total, got = 0;
    if (size == 0 || n == 0) return 0;
    if (n > (size_t)-1 / size) { errno = EOVERFLOW; f->flags |= F_ERR; return 0; }
    total = size * n;
    if (f->ungot >= 0) { p[got++] = (unsigned char)f->ungot; f->ungot = -1; }
    if (!read_ok(f)) return got / size;
    while (got < total && !(f->flags & F_EOF)) {
        size_t avail = f->rend - f->rpos;
        if (avail) {
            size_t k = avail < total - got ? avail : total - got;
            memcpy(p + got, f->buf + f->rpos, k);
            f->rpos += k; got += k;
            continue;
        }
        ensure_buf(f);
        if (total - got >= f->bufsize) {          /* big read: straight into the caller's buffer */
            ssize_t r;
            do r = be_read(f, p + got, total - got); while (r < 0 && errno == EINTR);
            if (r < 0) { f->flags |= F_ERR; break; }
            if (r == 0) { f->flags |= F_EOF; break; }
            got += (size_t)r;
            continue;
        }
        if (fill(f) != 0) break;
    }
    return got / size;
}

char *fgets(char *restrict buf, int n, FILE *restrict f)
{
    int i = 0;
    if (n <= 0) return NULL;
    while (i < n - 1) {
        if (f->ungot < 0 && f->rpos < f->rend) {  /* fast path: scan the buffer */
            size_t avail = f->rend - f->rpos, want = (size_t)(n - 1 - i), k = avail < want ? avail : want;
            unsigned char *nl = memchr(f->buf + f->rpos, '\n', k);
            if (nl) k = (size_t)(nl - (f->buf + f->rpos)) + 1;
            memcpy(buf + i, f->buf + f->rpos, k);
            f->rpos += k; i += (int)k;
            if (nl) break;
            continue;
        }
        {
            int c = fgetc(f);
            if (c == EOF) break;
            buf[i++] = (char)c;
            if (c == '\n') break;
        }
    }
    if (i == 0) return NULL;
    buf[i] = '\0';
    return buf;
}

ssize_t getdelim(char **restrict line, size_t *restrict cap, int delim, FILE *restrict f)
{
    size_t len = 0;
    if (!line || !cap) { errno = EINVAL; f->flags |= F_ERR; return -1; }
    if (!*line || *cap == 0) {
        char *p = realloc(*line, 128);
        if (!p) { f->flags |= F_ERR; return -1; }
        *line = p; *cap = 128;
    }
    for (;;) {
        int c;
        if (f->ungot < 0 && f->rpos < f->rend) {
            size_t avail = f->rend - f->rpos, k;
            unsigned char *d = memchr(f->buf + f->rpos, delim, avail);
            k = d ? (size_t)(d - (f->buf + f->rpos)) + 1 : avail;
            if (len + k + 1 > *cap) {
                size_t nc = *cap;
                char *p;
                while (len + k + 1 > nc) nc *= 2;
                p = realloc(*line, nc);
                if (!p) { f->flags |= F_ERR; return -1; }
                *line = p; *cap = nc;
            }
            memcpy(*line + len, f->buf + f->rpos, k);
            f->rpos += k; len += k;
            if (d) break;
            continue;
        }
        c = fgetc(f);
        if (c == EOF) { if (len == 0) return -1; break; }
        if (len + 2 > *cap) {
            char *p = realloc(*line, *cap * 2);
            if (!p) { f->flags |= F_ERR; return -1; }
            *line = p; *cap *= 2;
        }
        (*line)[len++] = (char)c;
        if (c == delim) break;
    }
    (*line)[len] = 0;
    if (len > (size_t)SSIZE_MAX) { errno = EOVERFLOW; return -1; }
    return (ssize_t)len;
}

ssize_t getline(char **restrict line, size_t *restrict cap, FILE *restrict f)
{
    return getdelim(line, cap, '\n', f);
}

/* ---- writing ----------------------------------------------------------- */

size_t fwrite(const void *restrict buf, size_t size, size_t n, FILE *restrict f)
{
    size_t total;
    if (size == 0 || n == 0) return 0;
    if (n > (size_t)-1 / size) { errno = EOVERFLOW; f->flags |= F_ERR; return 0; }
    total = size * n;
    return put_bytes(f, buf, total) == 0 ? n : 0;   /* partial counts are not tracked */
}

int fputc(int c, FILE *f)
{
    unsigned char ch = (unsigned char)c;
    return put_bytes(f, &ch, 1) == 0 ? ch : EOF;
}
int putc(int c, FILE *f) { return fputc(c, f); }
int putchar(int c)       { return fputc(c, stdout); }
int fputs(const char *restrict s, FILE *restrict f) { return put_bytes(f, s, strlen(s)) == 0 ? 1 : EOF; }
int puts(const char *s)
{
    return (put_bytes(stdout, s, strlen(s)) == 0 && put_bytes(stdout, "\n", 1) == 0) ? 1 : EOF;
}

/* The single-threaded library needs no locks: these are the plain calls. */
int getc_unlocked(FILE *f)          { return fgetc(f); }
int getchar_unlocked(void)          { return fgetc(stdin); }
int putc_unlocked(int c, FILE *f)   { return fputc(c, f); }
int putchar_unlocked(int c)         { return fputc(c, stdout); }
void flockfile(FILE *f)             { (void)f; }
int  ftrylockfile(FILE *f)          { (void)f; return 0; }
void funlockfile(FILE *f)           { (void)f; }

/* ---- positioning ------------------------------------------------------- */

int fseeko(FILE *f, off_t off, int whence)
{
    if (f->wlen && flush_write(f) != 0) return -1;
    if (whence == SEEK_CUR) {                 /* relative to the stream, not the backend */
        off -= (off_t)(f->rend - f->rpos) + (f->ungot >= 0 ? 1 : 0);
    }
    f->rpos = f->rend = 0;
    f->ungot = -1;
    if (be_seek(f, off, whence) < 0) return -1;
    f->flags &= ~F_EOF;
    return 0;
}
int fseek(FILE *f, long off, int whence) { return fseeko(f, off, whence); }

off_t ftello(FILE *f)
{
    off_t pos = be_seek(f, 0, SEEK_CUR);
    if (pos < 0) return -1;
    return pos + (off_t)f->wlen - (off_t)(f->rend - f->rpos) - (f->ungot >= 0 ? 1 : 0);
}
long ftell(FILE *f)
{
    off_t p = ftello(f);
    if (p > LONG_MAX) { errno = EOVERFLOW; return -1; }
    return (long)p;
}

int fgetpos(FILE *restrict f, fpos_t *restrict pos)
{
    off_t p = ftello(f);
    if (p < 0) return -1;
    pos->__pos = p;
    return 0;
}
int fsetpos(FILE *f, const fpos_t *pos) { return fseeko(f, pos->__pos, SEEK_SET); }

void rewind(FILE *f) { fseeko(f, 0, SEEK_SET); f->flags &= ~F_ERR; }
int  feof(FILE *f)   { return (f->flags & F_EOF) != 0; }
int  ferror(FILE *f) { return (f->flags & F_ERR) != 0; }
void clearerr(FILE *f) { f->flags &= ~(F_EOF | F_ERR); }
int  fileno(FILE *f) { if (f->fd < 0) { errno = EBADF; return -1; } return f->fd; }

int setvbuf(FILE *restrict f, char *restrict buf, int mode, size_t size)
{
    if ((mode != _IOFBF && mode != _IOLBF && mode != _IONBF) ||
        f->wlen || f->rend > f->rpos) { errno = EINVAL; return -1; }
    if (f->flags & F_OWNBUF) { free(f->buf); f->flags &= ~F_OWNBUF; f->buf = NULL; f->bufsize = 0; }
    f->bufmode = mode;
    if (mode != _IONBF && buf && size > 0) { f->buf = (unsigned char *)buf; f->bufsize = size; }
    else if (mode == _IONBF) { f->buf = NULL; f->bufsize = 0; }
    else if (f->buf && !(f->flags & F_OWNBUF)) { f->buf = NULL; f->bufsize = 0; }
    return 0;
}
void setbuf(FILE *restrict f, char *restrict buf) { setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ); }
void setlinebuf(FILE *f) { setvbuf(f, NULL, _IOLBF, 0); }

int remove(const char *path)
{
    if (unlink(path) == 0) return 0;
    if (errno == EISDIR || errno == EPERM) return rmdir(path);
    return -1;
}

/* ---- formatted output and input ------------------------------------------ */

typedef struct { FILE *f; int err; } stream_ctx_t;
static void stream_sink(void *c, const char *s, size_t n)
{
    stream_ctx_t *ctx = c;
    if (!ctx->err && put_bytes(ctx->f, s, n) != 0) ctx->err = 1;
}

int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap)
{
    stream_ctx_t ctx = { f, 0 };
    int r = novac_format(stream_sink, &ctx, fmt, ap);
    return ctx.err ? -1 : r;
}
int vprintf(const char *restrict fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }
int fprintf(FILE *restrict f, const char *restrict fmt, ...)
{ va_list ap; int r; va_start(ap, fmt); r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int printf(const char *restrict fmt, ...)
{ va_list ap; int r; va_start(ap, fmt); r = vfprintf(stdout, fmt, ap); va_end(ap); return r; }

/* dprintf: straight to a descriptor, through a small buffer. */
typedef struct { int fd, err; size_t n; char buf[256]; } fd_ctx_t;
static void fd_flush(fd_ctx_t *c)
{
    size_t off = 0;
    while (off < c->n && !c->err) {
        ssize_t w = write(c->fd, c->buf + off, c->n - off);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) { c->err = 1; break; }
        off += (size_t)w;
    }
    c->n = 0;
}
static void fd_sink(void *v, const char *s, size_t n)
{
    fd_ctx_t *c = v;
    while (n) {
        size_t k = sizeof c->buf - c->n;
        if (k > n) k = n;
        memcpy(c->buf + c->n, s, k);
        c->n += k; s += k; n -= k;
        if (c->n == sizeof c->buf) fd_flush(c);
    }
}
int vdprintf(int fd, const char *restrict fmt, va_list ap)
{
    fd_ctx_t c;
    int r;
    c.fd = fd; c.err = 0; c.n = 0;
    r = novac_format(fd_sink, &c, fmt, ap);
    fd_flush(&c);
    return c.err ? -1 : r;
}
int dprintf(int fd, const char *restrict fmt, ...)
{ va_list ap; int r; va_start(ap, fmt); r = vdprintf(fd, fmt, ap); va_end(ap); return r; }

static int f_scan_get(void *ctx) { return fgetc(ctx); }
static void f_scan_unget(int c, void *ctx) { ungetc(c, ctx); }
int vfscanf(FILE *restrict f, const char *restrict fmt, va_list ap)
{
    novac_scan_src src = { f_scan_get, f_scan_unget, f };
    return novac_scan(&src, fmt, ap);
}
int vscanf(const char *restrict fmt, va_list ap) { return vfscanf(stdin, fmt, ap); }
int fscanf(FILE *restrict f, const char *restrict fmt, ...)
{ va_list ap; int r; va_start(ap, fmt); r = vfscanf(f, fmt, ap); va_end(ap); return r; }
int scanf(const char *restrict fmt, ...)
{ va_list ap; int r; va_start(ap, fmt); r = vfscanf(stdin, fmt, ap); va_end(ap); return r; }

void perror(const char *s)
{
    int e = errno;
    if (s && *s) fprintf(stderr, "%s: ", s);
    fprintf(stderr, "%s\n", strerror(e));
}

/* ---- temporary files and pipes to commands ---------------------------- */

FILE *tmpfile(void)
{
    char path[] = "/tmp/tmpfXXXXXX";
    int fd = mkstemp(path);
    FILE *f;
    if (fd < 0) return NULL;
    unlink(path);
    f = fdopen(fd, "w+");
    if (!f) close(fd);
    return f;
}

FILE *popen(const char *cmd, const char *mode)
{
    int fds[2], child_end, parent_end, reading;
    pid_t pid;
    FILE *f;
    if (!mode || (mode[0] != 'r' && mode[0] != 'w') || (mode[1] && !(mode[1] == 'e' && !mode[2]))) {
        errno = EINVAL;
        return NULL;
    }
    reading = mode[0] == 'r';
    if (pipe2(fds, O_CLOEXEC) < 0) return NULL;
    parent_end = reading ? fds[0] : fds[1];
    child_end = reading ? fds[1] : fds[0];
    fflush(NULL);
    pid = fork();
    if (pid < 0) { int e = errno; close(fds[0]); close(fds[1]); errno = e; return NULL; }
    if (pid == 0) {
        char *argv[] = { (char *)"sh", (char *)"-c", (char *)cmd, NULL };
        int target = reading ? 1 : 0;
        if (child_end != target) dup2(child_end, target);
        else fcntl(child_end, F_SETFD, 0);      /* keep it across exec */
        execve("/bin/sh", argv, environ);
        _exit(127);
    }
    close(child_end);
    f = new_stream(parent_end, reading ? F_READ : F_WRITE);
    if (!f) { close(parent_end); return NULL; }
    if (mode[1] != 'e') fcntl(parent_end, F_SETFD, 0);
    f->pid = pid;
    return f;
}

int pclose(FILE *f)
{
    pid_t pid = f->pid;
    int status;
    if (!pid) { errno = ECHILD; return -1; }
    fclose(f);
    while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) return -1;
    return status;
}
