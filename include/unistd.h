#ifndef NOVAC_UNISTD_H
#define NOVAC_UNISTD_H
#include <stddef.h>
#include <sys/types.h>
#include <novac/defs.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define _SC_ARG_MAX        0
#define _SC_CHILD_MAX      1
#define _SC_CLK_TCK        2
#define _SC_NGROUPS_MAX    3
#define _SC_OPEN_MAX       4
#define _SC_PAGESIZE       30
#define _SC_PAGE_SIZE      _SC_PAGESIZE
#define _SC_NPROCESSORS_CONF 83
#define _SC_NPROCESSORS_ONLN 84
#define _SC_HOST_NAME_MAX  180
#define _SC_LINE_MAX       43

extern char **environ;
extern char *optarg;
extern int optind, opterr, optopt;

NOVAC_BEGIN_DECLS
ssize_t read(int fd, void *buf, size_t n);
ssize_t write(int fd, const void *buf, size_t n);
ssize_t pread(int fd, void *buf, size_t n, off_t off);
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off);
int     close(int fd);
off_t   lseek(int fd, off_t off, int whence);
int     fsync(int fd);
int     fdatasync(int fd);
int     unlink(const char *path);
int     unlinkat(int dirfd, const char *path, int flags);
int     rmdir(const char *path);
int     access(const char *path, int mode);
int     faccessat(int dirfd, const char *path, int mode, int flags);
int     chdir(const char *path);
int     fchdir(int fd);
char   *getcwd(char *buf, size_t size);
int     dup(int fd);
int     dup2(int from, int to);
int     dup3(int from, int to, int flags);
int     truncate(const char *path, off_t length);
int     ftruncate(int fd, off_t length);
int     symlink(const char *target, const char *linkpath);
int     symlinkat(const char *target, int dirfd, const char *linkpath);
ssize_t readlink(const char *restrict path, char *restrict buf, size_t size);
ssize_t readlinkat(int dirfd, const char *restrict path, char *restrict buf, size_t size);
int     isatty(int fd);
pid_t   getpid(void);
pid_t   getppid(void);
pid_t   setsid(void);
uid_t   getuid(void);
uid_t   geteuid(void);
gid_t   getgid(void);
gid_t   getegid(void);
int     pipe(int fds[2]);
int     pipe2(int fds[2], int flags);
/* Buffered stdio data is duplicated into the child: fflush(NULL) first, and
 * leave the child with _exit(), not exit(). */
pid_t   fork(void);
unsigned int sleep(unsigned int seconds);
int     usleep(unsigned long usec);
long    sysconf(int name);
int     getpagesize(void);
int     gethostname(char *name, size_t len);
_Noreturn void _exit(int status);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execv(const char *path, char *const argv[]);
int     execvp(const char *file, char *const argv[]);
int     execvpe(const char *file, char *const argv[], char *const envp[]);
/* The list forms: arguments end with a (char *)NULL; execle's envp follows it. */
int     execl(const char *path, const char *arg, ...);
int     execlp(const char *file, const char *arg, ...);
int     execle(const char *path, const char *arg, ...);
int     getopt(int argc, char *const argv[], const char *optstring);
NOVAC_END_DECLS
#endif
