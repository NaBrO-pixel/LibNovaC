#ifndef NOVAC_POLL_H
#define NOVAC_POLL_H
#include <time.h>
#include <signal.h>
#include <novac/defs.h>

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#define POLLRDHUP  0x2000

typedef unsigned long nfds_t;
struct pollfd { int fd; short events, revents; };

NOVAC_BEGIN_DECLS
int poll(struct pollfd *fds, nfds_t n, int timeout_ms);
int ppoll(struct pollfd *fds, nfds_t n, const struct timespec *timeout, const sigset_t *mask);
NOVAC_END_DECLS
#endif
