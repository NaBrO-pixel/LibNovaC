#ifndef NOVAC_TIME_H
#define NOVAC_TIME_H
#include <stddef.h>
#include <sys/types.h>
#include <novac/defs.h>

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7
#define TIMER_ABSTIME            1
#define TIME_UTC                 1
#define CLOCKS_PER_SEC           1000000L
typedef int clockid_t;
#ifndef NOVAC_CLOCK_T
#define NOVAC_CLOCK_T
typedef long clock_t;
#endif

struct timespec { time_t tv_sec; long tv_nsec; };
struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
    long tm_gmtoff;            /* seconds east of UTC: always 0 */
    const char *tm_zone;       /* "UTC" */
};

/* Local time is UTC: NovarisOS has no time zone database. */
extern char *tzname[2];
extern long timezone;
extern int daylight;

NOVAC_BEGIN_DECLS
time_t  time(time_t *t);
clock_t clock(void);
int     clock_gettime(clockid_t id, struct timespec *ts);
int     clock_getres(clockid_t id, struct timespec *ts);
int     nanosleep(const struct timespec *req, struct timespec *rem);
int     clock_nanosleep(clockid_t id, int flags, const struct timespec *req, struct timespec *rem);
int     timespec_get(struct timespec *ts, int base);
double  difftime(time_t end, time_t start);
struct tm *gmtime_r(const time_t *restrict t, struct tm *restrict out);
struct tm *gmtime(const time_t *t);
struct tm *localtime_r(const time_t *restrict t, struct tm *restrict out);
struct tm *localtime(const time_t *t);
time_t  timegm(struct tm *tm);
time_t  mktime(struct tm *tm);   /* same as timegm: local time == UTC */
void    tzset(void);
char   *asctime(const struct tm *tm);
char   *asctime_r(const struct tm *restrict tm, char *restrict buf);
char   *ctime(const time_t *t);
char   *ctime_r(const time_t *t, char *buf);
/* C99 and POSIX conversions plus GNU's %k %l %P %s and the '-' flag; the
 * E and O modifiers are accepted and ignored ("C" locale). */
size_t  strftime(char *restrict s, size_t max, const char *restrict fmt, const struct tm *restrict tm);
NOVAC_END_DECLS
#endif
