/* <string.h> and <strings.h>.
 *
 * Built with -ffreestanding -fno-builtin (and, on GCC, -fno-tree-loop-
 * distribute-patterns) so the compiler cannot turn these loops into calls
 * to the very functions being defined. The hot functions move eight bytes
 * at a time through `word_t`, a may_alias type that is safe for unaligned
 * access; strlen and memchr read whole aligned words, which can never cross
 * a page, and are exempt from AddressSanitizer for that reason only. */
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

typedef uint64_t __attribute__((may_alias, aligned(1))) word_t;
#define ONES  0x0101010101010101ULL
#define HIGHS 0x8080808080808080ULL
#define HAS_ZERO(w) (((w) - ONES) & ~(w) & HIGHS)

#if defined(__has_attribute)
# if __has_attribute(no_sanitize_address)
#  define NO_ASAN __attribute__((no_sanitize_address))
# endif
#endif
#ifndef NO_ASAN
# define NO_ASAN
#endif

/* ---- memory ------------------------------------------------------------- */

void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (; n >= 32; n -= 32, d += 32, s += 32) {
        word_t a = ((const word_t *)s)[0], b = ((const word_t *)s)[1];
        word_t c = ((const word_t *)s)[2], e = ((const word_t *)s)[3];
        ((word_t *)d)[0] = a; ((word_t *)d)[1] = b;
        ((word_t *)d)[2] = c; ((word_t *)d)[3] = e;
    }
    for (; n >= 8; n -= 8, d += 8, s += 8) *(word_t *)d = *(const word_t *)s;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    /* Compare as integers: relational comparison of unrelated pointers is UB. */
    if ((uintptr_t)d - (uintptr_t)s >= n) {        /* no harmful overlap going forward */
        for (; n >= 8; n -= 8, d += 8, s += 8) { word_t w = *(const word_t *)s; *(word_t *)d = w; }
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        for (; n >= 8; n -= 8) { d -= 8; s -= 8; { word_t w = *(const word_t *)s; *(word_t *)d = w; } }
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    uint64_t w = (unsigned char)c * ONES;
    for (; n >= 32; n -= 32, d += 32) {
        ((word_t *)d)[0] = w; ((word_t *)d)[1] = w; ((word_t *)d)[2] = w; ((word_t *)d)[3] = w;
    }
    for (; n >= 8; n -= 8, d += 8) *(word_t *)d = w;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n >= 8 && *(const word_t *)x == *(const word_t *)y; n -= 8, x += 8, y += 8) {}
    for (; n; n--, x++, y++)
        if (*x != *y) return *x < *y ? -1 : 1;
    return 0;
}

NO_ASAN void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    unsigned char ch = (unsigned char)c;
    uint64_t pat = ch * ONES;
    for (; n && ((uintptr_t)p & 7); n--, p++) if (*p == ch) return (void *)p;
    for (; n >= 8; n -= 8, p += 8) { uint64_t w = *(const uint64_t *)p ^ pat; if (HAS_ZERO(w)) break; }
    for (; n; n--, p++) if (*p == ch) return (void *)p;
    return NULL;
}

void *memrchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--) if (*--p == (unsigned char)c) return (void *)p;
    return NULL;
}

void *memccpy(void *restrict dst, const void *restrict src, int c, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (; n; n--) if ((*d++ = *s++) == (unsigned char)c) return d;
    return NULL;
}

void *mempcpy(void *restrict dst, const void *restrict src, size_t n)
{
    return (unsigned char *)memcpy(dst, src, n) + n;
}

void *memmem(const void *h, size_t hl, const void *n, size_t nl)
{
    const unsigned char *hp = h, *np = n, *end;
    if (nl == 0) return (void *)h;
    if (hl < nl) return NULL;
    end = hp + (hl - nl);
    for (; hp <= end; hp++) {
        hp = memchr(hp, np[0], (size_t)(end - hp) + 1);
        if (!hp) return NULL;
        if (memcmp(hp, np, nl) == 0) return (void *)hp;
    }
    return NULL;
}

/* Stores that the optimiser may not remove, for wiping secrets. */
void explicit_bzero(void *p, size_t n)
{
    memset(p, 0, n);
    __asm__ volatile ("" : : "r"(p) : "memory");
}

/* ---- strings ------------------------------------------------------------ */

NO_ASAN size_t strlen(const char *s)
{
    const char *p = s;
    for (; (uintptr_t)p & 7; p++) if (!*p) return (size_t)(p - s);
    for (;; p += 8) { uint64_t w = *(const uint64_t *)p; if (HAS_ZERO(w)) break; }
    while (*p) p++;
    return (size_t)(p - s);
}

size_t strnlen(const char *s, size_t maxlen)
{
    const char *z = memchr(s, 0, maxlen);
    return z ? (size_t)(z - s) : maxlen;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a < (unsigned char)*b ? -1
         : (unsigned char)*a > (unsigned char)*b ? 1 : 0;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        if (!*a) return 0;
    }
    return 0;
}

/* The "C" locale's collation is byte order. */
int strcoll(const char *a, const char *b) { return strcmp(a, b); }

size_t strxfrm(char *restrict dst, const char *restrict src, size_t n)
{
    size_t len = strlen(src);
    if (n) { size_t k = len < n - 1 ? len : n - 1; memcpy(dst, src, k); dst[k] = 0; }
    return len;
}

char *strcpy(char *restrict dst, const char *restrict src)
{
    memcpy(dst, src, strlen(src) + 1);
    return dst;
}

char *stpcpy(char *restrict dst, const char *restrict src)
{
    size_t n = strlen(src);
    memcpy(dst, src, n + 1);
    return dst + n;
}

char *strncpy(char *restrict dst, const char *restrict src, size_t n)
{
    size_t k = strnlen(src, n);
    memcpy(dst, src, k);
    memset(dst + k, 0, n - k);                 /* pad, as the standard requires */
    return dst;
}

char *stpncpy(char *restrict dst, const char *restrict src, size_t n)
{
    size_t k = strnlen(src, n);
    memcpy(dst, src, k);
    memset(dst + k, 0, n - k);
    return dst + k;
}

size_t strlcpy(char *restrict dst, const char *restrict src, size_t n)
{
    size_t len = strlen(src);
    if (n) { size_t k = len < n - 1 ? len : n - 1; memcpy(dst, src, k); dst[k] = 0; }
    return len;
}

size_t strlcat(char *restrict dst, const char *restrict src, size_t n)
{
    size_t dl = strnlen(dst, n);
    if (dl == n) return n + strlen(src);
    return dl + strlcpy(dst + dl, src, n - dl);
}

char *strcat(char *restrict dst, const char *restrict src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char *strncat(char *restrict dst, const char *restrict src, size_t n)
{
    char *d = dst + strlen(dst);
    size_t k = strnlen(src, n);
    memcpy(d, src, k);
    d[k] = '\0';
    return dst;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;   /* also matches the terminator */
        if (!*s) return NULL;
    }
}

char *strchrnul(const char *s, int c)
{
    while (*s && *s != (char)c) s++;
    return (char *)s;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    for (;; s++) {
        if (*s == (char)c) last = s;
        if (!*s) return (char *)last;
    }
}

char *strstr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    if (nl == 0) return (char *)h;
    return memmem(h, strlen(h), n, nl);
}

char *strcasestr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    for (; *h; h++) if (strncasecmp(h, n, nl) == 0) return (char *)h;
    return nl == 0 ? (char *)h : NULL;
}

/* A 256-bit membership set for the span functions. */
typedef struct { uint64_t w[4]; } cset;
static void cset_build(cset *s, const char *chars)
{
    s->w[0] = s->w[1] = s->w[2] = s->w[3] = 0;
    for (; *chars; chars++) { unsigned char c = (unsigned char)*chars; s->w[c >> 6] |= 1ULL << (c & 63); }
}
static int cset_has(const cset *s, unsigned char c) { return (s->w[c >> 6] >> (c & 63)) & 1; }

size_t strspn(const char *s, const char *accept)
{
    cset set; const char *p = s;
    cset_build(&set, accept);
    while (*p && cset_has(&set, (unsigned char)*p)) p++;
    return (size_t)(p - s);
}

size_t strcspn(const char *s, const char *reject)
{
    cset set; const char *p = s;
    cset_build(&set, reject);
    while (*p && !cset_has(&set, (unsigned char)*p)) p++;
    return (size_t)(p - s);
}

char *strpbrk(const char *s, const char *accept)
{
    s += strcspn(s, accept);
    return *s ? (char *)s : NULL;
}

char *strtok_r(char *restrict s, const char *restrict delim, char **restrict save)
{
    char *end;
    if (!s) s = *save;
    if (!s) return NULL;
    s += strspn(s, delim);
    if (!*s) { *save = NULL; return NULL; }
    end = s + strcspn(s, delim);
    if (*end) { *end = 0; *save = end + 1; }
    else *save = NULL;
    return s;
}

char *strtok(char *restrict s, const char *restrict delim)
{
    static char *save;
    return strtok_r(s, delim, &save);
}

char *strsep(char **sp, const char *delim)
{
    char *s = *sp, *end;
    if (!s) return NULL;
    end = s + strcspn(s, delim);
    if (*end) { *end = 0; *sp = end + 1; }
    else *sp = NULL;
    return s;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    return p ? memcpy(p, s, n) : NULL;
}

char *strndup(const char *s, size_t n)
{
    size_t k = strnlen(s, n);
    char *p = malloc(k + 1);
    if (!p) return NULL;
    memcpy(p, s, k);
    p[k] = 0;
    return p;
}

/* ---- errors and signals ------------------------------------------------- */

#include "strerror_table.h"

char *strerror(int e)
{
    if (e >= 0 && (size_t)e < sizeof novac_errmsg / sizeof novac_errmsg[0] && novac_errmsg[e])
        return (char *)novac_errmsg[e];
    return (char *)"Unknown error";   /* callers must not modify, as with any strerror */
}

/* POSIX (XSI) strerror_r: 0, or ERANGE if buf is too small. */
int strerror_r(int e, char *buf, size_t n)
{
    const char *m = strerror(e);
    size_t len = strlen(m);
    if (n == 0) return ERANGE;
    if (len >= n) { memcpy(buf, m, n - 1); buf[n - 1] = 0; return ERANGE; }
    memcpy(buf, m, len + 1);
    return 0;
}

char *strsignal(int sig)
{
    static const char *const names[] = {
        [SIGHUP] = "Hangup", [SIGINT] = "Interrupt", [SIGQUIT] = "Quit",
        [SIGILL] = "Illegal instruction", [SIGTRAP] = "Trace/breakpoint trap",
        [SIGABRT] = "Aborted", [SIGBUS] = "Bus error",
        [SIGFPE] = "Floating point exception", [SIGKILL] = "Killed",
        [SIGUSR1] = "User defined signal 1", [SIGSEGV] = "Segmentation fault",
        [SIGUSR2] = "User defined signal 2", [SIGPIPE] = "Broken pipe",
        [SIGALRM] = "Alarm clock", [SIGTERM] = "Terminated",
        [SIGSTKFLT] = "Stack fault", [SIGCHLD] = "Child exited",
        [SIGCONT] = "Continued", [SIGSTOP] = "Stopped (signal)",
        [SIGTSTP] = "Stopped", [SIGTTIN] = "Stopped (tty input)",
        [SIGTTOU] = "Stopped (tty output)", [SIGURG] = "Urgent I/O condition",
        [SIGXCPU] = "CPU time limit exceeded", [SIGXFSZ] = "File size limit exceeded",
        [SIGVTALRM] = "Virtual timer expired", [SIGPROF] = "Profiling timer expired",
        [SIGWINCH] = "Window changed", [SIGIO] = "I/O possible",
        [SIGPWR] = "Power failure", [SIGSYS] = "Bad system call",
    };
    if (sig > 0 && (size_t)sig < sizeof names / sizeof names[0] && names[sig]) return (char *)names[sig];
    if (sig >= SIGRTMIN && sig <= SIGRTMAX) return (char *)"Real-time signal";
    return (char *)"Unknown signal";
}

/* ---- <strings.h> -------------------------------------------------------- */

int strcasecmp(const char *a, const char *b)
{
    int x, y;
    do { x = tolower((unsigned char)*a++); y = tolower((unsigned char)*b++); } while (x && x == y);
    return x - y;
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    int x = 0, y = 0;
    while (n--) {
        x = tolower((unsigned char)*a++); y = tolower((unsigned char)*b++);
        if (!x || x != y) break;
    }
    return x - y;
}

void bzero(void *p, size_t n) { memset(p, 0, n); }
int  bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
void bcopy(const void *src, void *dst, size_t n) { memmove(dst, src, n); }

int ffs(int v)        { return v ? __builtin_ctz((unsigned)v) + 1 : 0; }
int ffsl(long v)      { return v ? __builtin_ctzl((unsigned long)v) + 1 : 0; }
int ffsll(long long v){ return v ? __builtin_ctzll((unsigned long long)v) + 1 : 0; }
