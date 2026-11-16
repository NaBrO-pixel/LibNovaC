#ifndef NOVAC_FCNTL_H
#define NOVAC_FCNTL_H
#include <sys/types.h>
#include <novac/defs.h>

/* Linux x86-64 values. */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_ACCMODE   3
#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_NDELAY    O_NONBLOCK
#define O_DSYNC     010000
#define O_SYNC      04010000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000
#define O_PATH      010000000

#define F_DUPFD          0
#define F_GETFD          1
#define F_SETFD          2
#define F_GETFL          3
#define F_SETFL          4
#define F_DUPFD_CLOEXEC  1030
#define FD_CLOEXEC       1

#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EACCESS          0x200
#define AT_EMPTY_PATH       0x1000

NOVAC_BEGIN_DECLS
/* open(path, flags) or open(path, flags, mode); mode is read only with O_CREAT. */
int open(const char *path, int flags, ...);
int openat(int dirfd, const char *path, int flags, ...);
int creat(const char *path, mode_t mode);
/* fcntl(fd, cmd) or fcntl(fd, cmd, int arg). */
int fcntl(int fd, int cmd, ...);
NOVAC_END_DECLS
#endif
