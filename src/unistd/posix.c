/* The rest of the POSIX system interface that NovarisOS's kernel offers:
 * positioned and vectored I/O, file metadata, *at() calls, memory
 * mapping, polling, identity, limits, randomness, terminals. Thin wrappers;
 * errors are -1 with errno. */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/random.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sched.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include "../internal/syscall.h"

ssize_t pread(int fd, void *b, size_t n, off_t o)        { return novac_sys4(NOVAC_SYS_pread64, fd, b, n, o); }
ssize_t pwrite(int fd, const void *b, size_t n, off_t o) { return novac_sys4(NOVAC_SYS_pwrite64, fd, b, n, o); }
ssize_t readv(int fd, const struct iovec *v, int c)      { return novac_sys3(NOVAC_SYS_readv, fd, v, c); }
ssize_t writev(int fd, const struct iovec *v, int c)     { return novac_sys3(NOVAC_SYS_writev, fd, v, c); }
int fsync(int fd)                     { return (int)novac_sys1(NOVAC_SYS_fsync, fd); }
int fdatasync(int fd)                 { return (int)novac_sys1(NOVAC_SYS_fdatasync, fd); }
int truncate(const char *p, off_t n)  { return (int)novac_sys2(NOVAC_SYS_truncate, p, n); }
int fchdir(int fd)                    { return (int)novac_sys1(NOVAC_SYS_fchdir, fd); }
int dup3(int a, int b, int f)         { return (int)novac_sys3(NOVAC_SYS_dup3, a, b, f); }
int symlink(const char *t, const char *l)            { return (int)novac_sys2(NOVAC_SYS_symlink, t, l); }
int symlinkat(const char *t, int d, const char *l)   { return (int)novac_sys3(NOVAC_SYS_symlinkat, t, d, l); }
ssize_t readlink(const char *restrict p, char *restrict b, size_t n) { return novac_sys3(NOVAC_SYS_readlink, p, b, n); }
ssize_t readlinkat(int d, const char *restrict p, char *restrict b, size_t n)
{ return novac_sys4(NOVAC_SYS_readlinkat, d, p, b, n); }
int unlinkat(int d, const char *p, int f)            { return (int)novac_sys3(NOVAC_SYS_unlinkat, d, p, f); }
int faccessat(int d, const char *p, int m, int f)
{
    /* the plain faccessat call has no flags argument; AT_EACCESS needs faccessat2 */
    if (f) { errno = EINVAL; return -1; }
    return (int)novac_sys3(NOVAC_SYS_faccessat, d, p, m);
}
int renameat(int od, const char *o, int nd, const char *n) { return (int)novac_sys4(NOVAC_SYS_renameat, od, o, nd, n); }

pid_t setsid(void)  { return (pid_t)novac_ret(novac_syscall6(NOVAC_SYS_setsid, 0, 0, 0, 0, 0, 0)); }
uid_t getuid(void)  { return (uid_t)novac_syscall6(NOVAC_SYS_getuid, 0, 0, 0, 0, 0, 0); }
uid_t geteuid(void) { return (uid_t)novac_syscall6(NOVAC_SYS_geteuid, 0, 0, 0, 0, 0, 0); }
gid_t getgid(void)  { return (gid_t)novac_syscall6(NOVAC_SYS_getgid, 0, 0, 0, 0, 0, 0); }
gid_t getegid(void) { return (gid_t)novac_syscall6(NOVAC_SYS_getegid, 0, 0, 0, 0, 0, 0); }
int sched_yield(void) { return (int)novac_ret(novac_syscall6(NOVAC_SYS_sched_yield, 0, 0, 0, 0, 0, 0)); }

/* ---- files ------------------------------------------------------------- */

int openat(int dirfd, const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return (int)novac_sys4(NOVAC_SYS_openat, dirfd, path, flags, mode);
}

int creat(const char *path, mode_t mode) { return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode); }

int fcntl(int fd, int cmd, ...)
{
    long arg = 0;
    va_list ap;
    va_start(ap, cmd);
    if (cmd == F_DUPFD || cmd == F_SETFD || cmd == F_SETFL || cmd == F_DUPFD_CLOEXEC) arg = va_arg(ap, int);
    else arg = (long)va_arg(ap, void *);       /* commands taking a pointer, or nothing */
    va_end(ap);
    return (int)novac_sys3(NOVAC_SYS_fcntl, fd, cmd, arg);
}

int fstatat(int d, const char *p, struct stat *st, int f) { return (int)novac_sys4(NOVAC_SYS_newfstatat, d, p, st, f); }
int mkdirat(int d, const char *p, mode_t m)  { return (int)novac_sys3(NOVAC_SYS_mkdirat, d, p, m); }
int chmod(const char *p, mode_t m)           { return (int)novac_sys2(NOVAC_SYS_chmod, p, m); }
int fchmod(int fd, mode_t m)                 { return (int)novac_sys2(NOVAC_SYS_fchmod, fd, m); }
mode_t umask(mode_t m)                       { return (mode_t)novac_syscall6(NOVAC_SYS_umask, m, 0, 0, 0, 0, 0); }
int utimensat(int d, const char *p, const struct timespec t[2], int f)
{ return (int)novac_sys4(NOVAC_SYS_utimensat, d, p, t, f); }
int futimens(int fd, const struct timespec t[2]) { return utimensat(fd, NULL, t, 0); }
int utimes(const char *p, const struct timeval tv[2])
{
    struct timespec ts[2];
    if (!tv) return utimensat(AT_FDCWD, p, NULL, 0);
    ts[0].tv_sec = tv[0].tv_sec; ts[0].tv_nsec = tv[0].tv_usec * 1000;
    ts[1].tv_sec = tv[1].tv_sec; ts[1].tv_nsec = tv[1].tv_usec * 1000;
    return utimensat(AT_FDCWD, p, ts, 0);
}

/* ---- memory ------------------------------------------------------------ */

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    long r = novac_syscall6(NOVAC_SYS_mmap, (long)addr, (long)len, prot, flags, fd, off);
    if (r < 0 && r >= -NOVAC_MAX_ERRNO) { errno = (int)-r; return MAP_FAILED; }
    return (void *)r;
}
int munmap(void *a, size_t n)          { return (int)novac_sys2(NOVAC_SYS_munmap, a, n); }
int mprotect(void *a, size_t n, int p) { return (int)novac_sys3(NOVAC_SYS_mprotect, a, n, p); }
int msync(void *a, size_t n, int f)    { return (int)novac_sys3(NOVAC_SYS_msync, a, n, f); }
int madvise(void *a, size_t n, int v)  { return (int)novac_sys3(NOVAC_SYS_madvise, a, n, v); }

/* ---- waiting for descriptors ------------------------------------------- */

int ppoll(struct pollfd *fds, nfds_t n, const struct timespec *t, const sigset_t *mask)
{ return (int)novac_sys5(NOVAC_SYS_ppoll, fds, n, t, mask, sizeof(sigset_t)); }

int poll(struct pollfd *fds, nfds_t n, int ms)
{
    return (int)novac_sys3(NOVAC_SYS_poll, fds, n, ms);
}

int pselect(int n, fd_set *restrict r, fd_set *restrict w, fd_set *restrict e,
            const struct timespec *restrict t, const sigset_t *restrict mask)
{
    struct { const sigset_t *set; size_t size; } m = { mask, sizeof(sigset_t) };
    struct timespec copy, *tp = NULL;
    if (t) { copy = *t; tp = &copy; }          /* the kernel may write it back */
    return (int)novac_sys6(NOVAC_SYS_pselect6, n, r, w, e, tp, mask ? &m : NULL);
}

int select(int n, fd_set *restrict r, fd_set *restrict w, fd_set *restrict e, struct timeval *restrict tv)
{
    struct timespec ts, *tp = NULL;
    int rc;
    if (tv) {
        if (tv->tv_sec < 0 || tv->tv_usec < 0) { errno = EINVAL; return -1; }
        ts.tv_sec = tv->tv_sec + tv->tv_usec / 1000000;
        ts.tv_nsec = (tv->tv_usec % 1000000) * 1000;
        tp = &ts;
    }
    rc = (int)novac_sys6(NOVAC_SYS_pselect6, n, r, w, e, tp, 0);
    if (tv && rc >= 0) { tv->tv_sec = ts.tv_sec; tv->tv_usec = ts.tv_nsec / 1000; }   /* time left, as Linux does */
    return rc;
}

/* ---- system information and limits ---------------------------------------- */

int uname(struct utsname *u)          { return (int)novac_sys1(NOVAC_SYS_uname, u); }
clock_t times(struct tms *t)          { return (clock_t)novac_sys1(NOVAC_SYS_times, t); }
int getrusage(int who, struct rusage *u) { return (int)novac_sys2(NOVAC_SYS_getrusage, who, u); }
int prlimit(pid_t pid, int res, const struct rlimit *n, struct rlimit *o)
{ return (int)novac_sys4(NOVAC_SYS_prlimit64, pid, res, n, o); }
int getrlimit(int res, struct rlimit *r)       { return prlimit(0, res, NULL, r); }
int setrlimit(int res, const struct rlimit *r) { return prlimit(0, res, r, NULL); }

int gettimeofday(struct timeval *restrict tv, void *restrict tz)
{
    struct timespec ts;
    if (tz) { struct timezone *z = tz; z->tz_minuteswest = 0; z->tz_dsttime = 0; }
    if (!tv) return 0;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return -1;
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;
    return 0;
}

int gethostname(char *name, size_t len)
{
    struct utsname u;
    size_t n;
    if (uname(&u) != 0) return -1;
    n = strlen(u.nodename);
    if (n + 1 > len) { errno = ENAMETOOLONG; return -1; }
    memcpy(name, u.nodename, n + 1);
    return 0;
}

int getpagesize(void) { return 4096; }

long sysconf(int name)
{
    struct rlimit r;
    switch (name) {
    case _SC_PAGESIZE:   return 4096;
    case _SC_CLK_TCK:    return 100;
    case _SC_ARG_MAX:    return 131072;
    case _SC_CHILD_MAX:  return 1024;
    case _SC_NGROUPS_MAX: return 65536;
    case _SC_LINE_MAX:   return 2048;
    case _SC_HOST_NAME_MAX: return 64;
    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN: return 1;
    case _SC_OPEN_MAX:
        if (getrlimit(RLIMIT_NOFILE, &r) == 0 && r.rlim_cur != RLIM_INFINITY) return (long)r.rlim_cur;
        return 1024;
    default: errno = EINVAL; return -1;
    }
}

int pause(void)
{
    sigset_t cur;
    sigprocmask(SIG_BLOCK, NULL, &cur);
    return sigsuspend(&cur);
}

int usleep(unsigned long usec)
{
    struct timespec ts = { (time_t)(usec / 1000000), (long)(usec % 1000000) * 1000 };
    return nanosleep(&ts, NULL);
}

ssize_t getrandom(void *buf, size_t n, unsigned flags) { return novac_sys3(NOVAC_SYS_getrandom, buf, n, flags); }

int getentropy(void *buf, size_t n)
{
    unsigned char *p = buf;
    if (n > 256) { errno = EIO; return -1; }
    while (n) {
        ssize_t r = getrandom(p, n, 0);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        p += r; n -= (size_t)r;
    }
    return 0;
}

/* ---- terminals --------------------------------------------------------- */

int ioctl(int fd, unsigned long req, ...)
{
    va_list ap;
    void *arg;
    va_start(ap, req);
    arg = va_arg(ap, void *);
    va_end(ap);
    return (int)novac_sys3(NOVAC_SYS_ioctl, fd, req, arg);
}

int isatty(int fd)
{
    struct termios t;
    return ioctl(fd, TCGETS, &t) == 0;     /* errno ENOTTY otherwise, as POSIX says */
}

int tcgetattr(int fd, struct termios *t) { return ioctl(fd, TCGETS, t); }
int tcsetattr(int fd, int when, const struct termios *t)
{
    unsigned long req = when == TCSANOW ? TCSETS : when == TCSADRAIN ? TCSETSW : when == TCSAFLUSH ? TCSETSF : 0;
    if (!req) { errno = EINVAL; return -1; }
    return ioctl(fd, req, t);
}
void cfmakeraw(struct termios *t)
{
    t->c_iflag &= ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    t->c_oflag &= ~(tcflag_t)OPOST;
    t->c_lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t->c_cflag &= ~(tcflag_t)(CSIZE | PARENB);
    t->c_cflag |= CS8;
    t->c_cc[VMIN] = 1;
    t->c_cc[VTIME] = 0;
}
speed_t cfgetospeed(const struct termios *t) { return t->c_cflag & CBAUD; }
speed_t cfgetispeed(const struct termios *t) { return cfgetospeed(t); }
int cfsetospeed(struct termios *t, speed_t s)
{
    if (s & ~(speed_t)CBAUD) { errno = EINVAL; return -1; }
    t->c_cflag = (t->c_cflag & ~(tcflag_t)CBAUD) | s;
    return 0;
}
int cfsetispeed(struct termios *t, speed_t s) { return s ? cfsetospeed(t, s) : 0; }
int cfsetspeed(struct termios *t, speed_t s)  { return cfsetospeed(t, s); }
