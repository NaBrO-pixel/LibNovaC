/* POSIX-style file and process calls: thin, checked wrappers over the Linux
 * x86-64 system calls NovarisOS implements. Errors: -1 with errno set. */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include "../internal/sys.h"
#include "../internal/syscall.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(struct stat) == 144, "struct stat must match the Linux x86-64 layout");

ssize_t read(int fd, void *buf, size_t n)
{
    if (n > (size_t)LONG_MAX) { errno = EINVAL; return -1; }
    return novac_sys3(NOVAC_SYS_read, fd, buf, n);
}

ssize_t write(int fd, const void *buf, size_t n)
{
    if (n > (size_t)LONG_MAX) { errno = EINVAL; return -1; }
    return novac_sys3(NOVAC_SYS_write, fd, buf, n);
}

int open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return (int)novac_sys3(NOVAC_SYS_open, path, flags, mode);
}

int     close(int fd)                    { return (int)novac_sys1(NOVAC_SYS_close, fd); }
off_t   lseek(int fd, off_t off, int w)  { return novac_sys3(NOVAC_SYS_lseek, fd, off, w); }
int     unlink(const char *p)            { return (int)novac_sys1(NOVAC_SYS_unlink, p); }
int     rmdir(const char *p)             { return (int)novac_sys1(NOVAC_SYS_rmdir, p); }
int     access(const char *p, int m)     { return (int)novac_sys2(NOVAC_SYS_access, p, m); }
int     chdir(const char *p)             { return (int)novac_sys1(NOVAC_SYS_chdir, p); }
int     dup(int fd)                      { return (int)novac_sys1(NOVAC_SYS_dup, fd); }
int     dup2(int a, int b)               { return (int)novac_sys2(NOVAC_SYS_dup2, a, b); }
int     ftruncate(int fd, off_t len)     { return (int)novac_sys2(NOVAC_SYS_ftruncate, fd, len); }
pid_t   getpid(void)                     { return (pid_t)novac_syscall6(NOVAC_SYS_getpid, 0, 0, 0, 0, 0, 0); }
pid_t   getppid(void)                    { return (pid_t)novac_syscall6(NOVAC_SYS_getppid, 0, 0, 0, 0, 0, 0); }
int     pipe2(int fds[2], int flags)     { return (int)novac_sys2(NOVAC_SYS_pipe2, fds, flags); }
int     pipe(int fds[2])                 { return pipe2(fds, 0); }
pid_t   fork(void)                       { return (pid_t)novac_ret(novac_syscall6(NOVAC_SYS_fork, 0, 0, 0, 0, 0, 0)); }
int     stat(const char *p, struct stat *s)  { return (int)novac_sys2(NOVAC_SYS_stat, p, s); }
int     lstat(const char *p, struct stat *s) { return (int)novac_sys2(NOVAC_SYS_lstat, p, s); }
int     fstat(int fd, struct stat *s)    { return (int)novac_sys2(NOVAC_SYS_fstat, fd, s); }
int     mkdir(const char *p, mode_t m)   { return (int)novac_sys2(NOVAC_SYS_mkdir, p, m); }
int     rename(const char *a, const char *b) { return (int)novac_sys2(NOVAC_SYS_rename, a, b); }

char *getcwd(char *buf, size_t size)
{
    long r;
    if (!buf || size == 0) { errno = EINVAL; return NULL; }
    r = novac_sys2(NOVAC_SYS_getcwd, buf, size);   /* returns length incl. NUL */
    return r < 0 ? NULL : buf;
}

unsigned int sleep(unsigned int seconds)
{
    struct timespec req = { (time_t)seconds, 0 }, rem = { 0, 0 };
    if (nanosleep(&req, &rem) == 0) return 0;
    return errno == EINTR ? (unsigned int)rem.tv_sec : seconds;
}

_Noreturn void _exit(int status) { novac_sys_exit(status); }

pid_t waitpid(pid_t pid, int *status, int options)
{
    return (pid_t)novac_ret(novac_syscall6(NOVAC_SYS_wait4, pid, (long)status, options, 0, 0, 0));
}

pid_t wait(int *status) { return waitpid(-1, status, 0); }

/* --- running another program ------------------------------------------- */

int execve(const char *path, char *const argv[], char *const envp[])
{
    return (int)novac_sys3(NOVAC_SYS_execve, path, argv, envp);
}

int execv(const char *path, char *const argv[])
{
    return execve(path, argv, environ);
}

/* A name with a '/' is a path; any other is looked for in each directory
 * of PATH in turn, as the shell would (default "/bin:/usr/bin"). ENOENT
 * and ENOTDIR move on to the next directory; any other error is
 * remembered - EACCES above all - and is what the call fails with if
 * nothing runs. */
int execvpe(const char *file, char *const argv[], char *const envp[])
{
    const char *path, *p;
    char buf[PATH_MAX];
    int saw_eacces = 0, last = ENOENT;

    if (!file || !*file) { errno = ENOENT; return -1; }
    if (strchr(file, '/')) return execve(file, argv, envp);

    path = getenv("PATH");
    if (!path) path = "/bin:/usr/bin";
    for (p = path; ; ) {
        const char *end = strchr(p, ':');
        size_t dlen = end ? (size_t)(end - p) : strlen(p);
        size_t flen = strlen(file);
        if (dlen + 1 + flen + 1 <= sizeof buf) {
            size_t n = 0;
            if (dlen == 0) buf[n++] = '.';      /* an empty entry is the cwd */
            else { memcpy(buf, p, dlen); n = dlen; }
            buf[n++] = '/';
            memcpy(buf + n, file, flen + 1);
            execve(buf, argv, envp);
            if (errno == EACCES) saw_eacces = 1;
            else if (errno != ENOENT && errno != ENOTDIR) last = errno;
        }
        if (!end) break;
        p = end + 1;
    }
    errno = saw_eacces ? EACCES : last;
    return -1;
}

int execvp(const char *file, char *const argv[]) { return execvpe(file, argv, environ); }

/* The list forms gather their arguments into an array on the stack. */
static size_t count_args(const char *arg, va_list ap)
{
    size_t n = 1;
    if (!arg) return 0;
    while (va_arg(ap, const char *)) n++;
    return n;
}

#define GATHER(argv, arg, ap, n) do { size_t i_; argv[0] = (char *)(arg); \
    for (i_ = 1; i_ <= (n); i_++) argv[i_] = va_arg(ap, char *); } while (0)

int execl(const char *path, const char *arg, ...)
{
    va_list ap;
    size_t n;
    va_start(ap, arg); n = count_args(arg, ap); va_end(ap);
    {
        char *argv[n + 1];
        va_start(ap, arg); GATHER(argv, arg, ap, n); va_end(ap);
        argv[n] = NULL;
        return execve(path, argv, environ);
    }
}

int execlp(const char *file, const char *arg, ...)
{
    va_list ap;
    size_t n;
    va_start(ap, arg); n = count_args(arg, ap); va_end(ap);
    {
        char *argv[n + 1];
        va_start(ap, arg); GATHER(argv, arg, ap, n); va_end(ap);
        argv[n] = NULL;
        return execvp(file, argv);
    }
}

int execle(const char *path, const char *arg, ...)
{
    va_list ap;
    size_t n;
    char **envp;
    va_start(ap, arg); n = count_args(arg, ap); va_end(ap);
    {
        char *argv[n + 1];
        size_t i;
        va_start(ap, arg);
        argv[0] = (char *)arg;
        for (i = 1; i < n; i++) argv[i] = va_arg(ap, char *);
        if (n) (void)va_arg(ap, char *);       /* the terminating NULL */
        envp = va_arg(ap, char **);
        va_end(ap);
        argv[n] = NULL;
        return execve(path, argv, envp);
    }
}

