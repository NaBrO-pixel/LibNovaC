/* stdlib functions that need the kernel: system, temporary files,
 * realpath, arc4random, getauxval. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/random.h>
#include <sys/auxv.h>

int system(const char *cmd)
{
    struct sigaction ign, old_int, old_quit;
    sigset_t block, old_mask;
    pid_t pid;
    int status;

    if (!cmd) return access("/bin/sh", X_OK) == 0;
    /* POSIX: ignore SIGINT and SIGQUIT and block SIGCHLD while waiting */
    ign.sa_handler = SIG_IGN; ign.sa_flags = 0; sigemptyset(&ign.sa_mask);
    sigaction(SIGINT, &ign, &old_int);
    sigaction(SIGQUIT, &ign, &old_quit);
    sigemptyset(&block); sigaddset(&block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block, &old_mask);
    pid = fork();
    if (pid == 0) {
        char *argv[] = { (char *)"sh", (char *)"-c", (char *)cmd, NULL };
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGQUIT, &old_quit, NULL);
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        execve("/bin/sh", argv, environ);
        _exit(127);
    }
    if (pid < 0) status = -1;
    else while (waitpid(pid, &status, 0) < 0) if (errno != EINTR) { status = -1; break; }
    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGQUIT, &old_quit, NULL);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    return status;
}

/* ---- arc4random: bytes from getrandom, buffered; refilled after fork ---- */
static unsigned char rbuf[256];
static size_t rleft;
static pid_t rpid;

static void fill_random(void *buf, size_t n)
{
    unsigned char *p = buf;
    while (n) {
        ssize_t r = getrandom(p, n, 0);
        if (r < 0) { if (errno == EINTR) continue; abort(); }   /* no entropy: never fake it */
        p += r; n -= (size_t)r;
    }
}

void arc4random_buf(void *buf, size_t n)
{
    unsigned char *p = buf;
    pid_t me = getpid();
    if (me != rpid) { rleft = 0; rpid = me; }        /* a forked child must not repeat its parent */
    if (n >= sizeof rbuf) { fill_random(p, n); return; }
    while (n) {
        size_t k;
        if (rleft == 0) { fill_random(rbuf, sizeof rbuf); rleft = sizeof rbuf; }
        k = n < rleft ? n : rleft;
        memcpy(p, rbuf + sizeof rbuf - rleft, k);
        memset(rbuf + sizeof rbuf - rleft, 0, k);    /* used bytes are not kept */
        rleft -= k; p += k; n -= k;
    }
}

uint32_t arc4random(void)
{
    uint32_t v;
    arc4random_buf(&v, sizeof v);
    return v;
}

uint32_t arc4random_uniform(uint32_t upper)
{
    uint32_t min, r;
    if (upper < 2) return 0;
    min = (uint32_t)(-upper) % upper;                /* 2^32 mod upper: reject below it */
    do r = arc4random(); while (r < min);
    return r % upper;
}

/* ---- temporary files -------------------------------------------------- */

static int fill_template(char *tmpl, int suffixlen, char **x)
{
    size_t n = strlen(tmpl);
    if (suffixlen < 0 || n < 6 + (size_t)suffixlen ||
        memcmp(tmpl + n - suffixlen - 6, "XXXXXX", 6) != 0) { errno = EINVAL; return -1; }
    *x = tmpl + n - suffixlen - 6;
    return 0;
}

static void randomise(char *x)
{
    static const char set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int i;
    for (i = 0; i < 6; i++) x[i] = set[arc4random_uniform(sizeof set - 1)];
}

static int make_temp(char *tmpl, int suffixlen, int flags)
{
    char *x;
    int tries;
    if (fill_template(tmpl, suffixlen, &x) < 0) return -1;
    for (tries = 0; tries < 100; tries++) {
        int fd;
        randomise(x);
        fd = open(tmpl, O_RDWR | O_CREAT | O_EXCL | (flags & ~O_ACCMODE), 0600);
        if (fd >= 0) return fd;
        if (errno != EEXIST) return -1;
    }
    memcpy(x, "XXXXXX", 6);
    errno = EEXIST;
    return -1;
}

int mkstemp(char *tmpl)                    { return make_temp(tmpl, 0, 0); }
int mkostemp(char *tmpl, int flags)        { return make_temp(tmpl, 0, flags); }
int mkstemps(char *tmpl, int suffixlen)    { return make_temp(tmpl, suffixlen, 0); }

char *mkdtemp(char *tmpl)
{
    char *x;
    int tries;
    if (fill_template(tmpl, 0, &x) < 0) return NULL;
    for (tries = 0; tries < 100; tries++) {
        randomise(x);
        if (mkdir(tmpl, 0700) == 0) return tmpl;
        if (errno != EEXIST) return NULL;
    }
    memcpy(x, "XXXXXX", 6);
    return NULL;
}

/* ---- realpath ---------------------------------------------------------- */

char *realpath(const char *restrict path, char *restrict resolved)
{
    char out[PATH_MAX], rest[PATH_MAX], link[PATH_MAX];
    size_t olen, links = 0;
    char *p;

    if (!path) { errno = EINVAL; return NULL; }
    if (!*path) { errno = ENOENT; return NULL; }
    if (strlen(path) >= sizeof rest) { errno = ENAMETOOLONG; return NULL; }
    strcpy(rest, path);
    if (rest[0] == '/') { out[0] = '/'; out[1] = 0; olen = 1; }
    else {
        if (!getcwd(out, sizeof out)) return NULL;
        olen = strlen(out);
    }
    p = rest;
    while (*p) {
        char *comp, *end;
        size_t clen;
        struct stat st;
        while (*p == '/') p++;
        if (!*p) break;
        comp = p;
        end = strchr(p, '/');
        clen = end ? (size_t)(end - p) : strlen(p);
        p += clen;
        if (clen == 1 && comp[0] == '.') continue;
        if (clen == 2 && comp[0] == '.' && comp[1] == '.') {
            while (olen > 1 && out[olen - 1] != '/') olen--;
            if (olen > 1) olen--;                    /* drop the slash too, unless it is the root */
            out[olen] = 0;
            continue;
        }
        if (olen + (olen > 1) + clen + 1 > sizeof out) { errno = ENAMETOOLONG; return NULL; }
        if (olen > 1) out[olen++] = '/';
        memcpy(out + olen, comp, clen);
        olen += clen;
        out[olen] = 0;
        if (lstat(out, &st) != 0) return NULL;
        if (S_ISLNK(st.st_mode)) {
            ssize_t n;
            size_t restlen = strlen(p);
            if (++links > SYMLOOP_MAX) { errno = ELOOP; return NULL; }
            n = readlink(out, link, sizeof link - 1);
            if (n < 0) return NULL;
            link[n] = 0;
            if ((size_t)n + restlen + 1 > sizeof rest) { errno = ENAMETOOLONG; return NULL; }
            memmove(rest + n, p, restlen + 1);       /* rest = target + what was left ("/..." or "") */
            memcpy(rest, link, (size_t)n);
            p = rest;
            if (link[0] == '/') { out[0] = '/'; out[1] = 0; olen = 1; }
            else {                                   /* relative to the link's directory */
                while (olen > 1 && out[olen - 1] != '/') olen--;
                if (olen > 1) olen--;
                out[olen] = 0;
            }
        } else if (*p && !S_ISDIR(st.st_mode)) {
            errno = ENOTDIR;
            return NULL;
        }
    }
    if (!resolved) {
        resolved = malloc(olen + 1);
        if (!resolved) return NULL;
    }
    memcpy(resolved, out, olen + 1);
    return resolved;
}

/* ---- the auxiliary vector ----------------------------------------------- */

extern unsigned long *novac__auxv __attribute__((weak));

unsigned long getauxval(unsigned long type)
{
    unsigned long *a = novac__auxv;
    if (a) for (; a[0] != AT_NULL; a += 2) if (a[0] == type) return a[1];
    errno = ENOENT;
    return 0;
}
