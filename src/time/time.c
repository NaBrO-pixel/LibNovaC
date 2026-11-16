/* <time.h>'s clocks: the system-call half (the calendar is calendar.c). */
#include <time.h>
#include <errno.h>
#include <limits.h>
#include "../internal/syscall.h"

int clock_gettime(clockid_t id, struct timespec *ts)
{ return (int)novac_sys2(NOVAC_SYS_clock_gettime, id, ts); }

int nanosleep(const struct timespec *req, struct timespec *rem)
{ return (int)novac_sys2(NOVAC_SYS_nanosleep, req, rem); }

time_t time(time_t *out)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return (time_t)-1;
    if (out) *out = ts.tv_sec;
    return ts.tv_sec;
}

int clock_getres(clockid_t id, struct timespec *ts)
{ return (int)novac_sys2(NOVAC_SYS_clock_getres, id, ts); }

/* Returns the error number, not -1 (POSIX). */
int clock_nanosleep(clockid_t id, int flags, const struct timespec *req, struct timespec *rem)
{
    long r = novac_syscall6(NOVAC_SYS_clock_nanosleep, id, flags, (long)req, (long)rem, 0, 0);
    return r < 0 ? (int)-r : 0;
}

int timespec_get(struct timespec *ts, int base)
{
    if (base != TIME_UTC || clock_gettime(CLOCK_REALTIME, ts) != 0) return 0;
    return base;
}

/* Processor time used, from times(): user + system, in CLOCKS_PER_SEC. */
#include <sys/times.h>
clock_t clock(void)
{
    struct tms t;
    if (times(&t) == (clock_t)-1) return (clock_t)-1;
    return (t.tms_utime + t.tms_stime) * (CLOCKS_PER_SEC / 100);
}
