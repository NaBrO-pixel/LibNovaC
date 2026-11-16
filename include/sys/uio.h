#ifndef NOVAC_SYS_UIO_H
#define NOVAC_SYS_UIO_H
#include <sys/types.h>
#include <novac/defs.h>

struct iovec { void *iov_base; size_t iov_len; };
#define IOV_MAX 1024

NOVAC_BEGIN_DECLS
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
NOVAC_END_DECLS
#endif
