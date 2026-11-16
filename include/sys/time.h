#ifndef NOVAC_SYS_TIME_H
#define NOVAC_SYS_TIME_H
#include <sys/types.h>
#include <time.h>
#include <sys/select.h>
#include <novac/defs.h>

typedef long suseconds_t;
struct timeval { time_t tv_sec; suseconds_t tv_usec; };
struct timezone { int tz_minuteswest, tz_dsttime; };

#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timercmp(a, b, CMP) (((a)->tv_sec == (b)->tv_sec) ? \
    ((a)->tv_usec CMP (b)->tv_usec) : ((a)->tv_sec CMP (b)->tv_sec))
#define timeradd(a, b, r) do { (r)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
    (r)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
    if ((r)->tv_usec >= 1000000) { (r)->tv_sec++; (r)->tv_usec -= 1000000; } } while (0)
#define timersub(a, b, r) do { (r)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (r)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((r)->tv_usec < 0) { (r)->tv_sec--; (r)->tv_usec += 1000000; } } while (0)

NOVAC_BEGIN_DECLS
int gettimeofday(struct timeval *restrict tv, void *restrict tz);
int utimes(const char *path, const struct timeval times[2]);
NOVAC_END_DECLS
#endif
