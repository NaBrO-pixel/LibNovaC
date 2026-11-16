#ifndef NOVAC_SYS_SELECT_H
#define NOVAC_SYS_SELECT_H
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <novac/defs.h>

#define FD_SETSIZE 1024
typedef struct { unsigned long fds_bits[FD_SETSIZE / 64]; } fd_set;
#define FD_ZERO(s)  do { unsigned novac_i_; for (novac_i_ = 0; novac_i_ < FD_SETSIZE / 64; novac_i_++) (s)->fds_bits[novac_i_] = 0; } while (0)
#define FD_SET(d, s)   ((s)->fds_bits[(d) / 64] |= (1UL << ((d) % 64)))
#define FD_CLR(d, s)   ((s)->fds_bits[(d) / 64] &= ~(1UL << ((d) % 64)))
#define FD_ISSET(d, s) (((s)->fds_bits[(d) / 64] & (1UL << ((d) % 64))) != 0)

struct timeval;
NOVAC_BEGIN_DECLS
int select(int n, fd_set *restrict r, fd_set *restrict w, fd_set *restrict e,
           struct timeval *restrict timeout);
int pselect(int n, fd_set *restrict r, fd_set *restrict w, fd_set *restrict e,
            const struct timespec *restrict timeout, const sigset_t *restrict mask);
NOVAC_END_DECLS
#endif
