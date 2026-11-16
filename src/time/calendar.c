/* <time.h>'s calendar half: UTC arithmetic, the "local" forms (local time
 * is UTC: NovarisOS has no time zone database), and strftime. No system
 * calls, so it is part of the portable core and host-tested. */
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

double difftime(time_t end, time_t start) { return (double)end - (double)start; }

static long floor_div(long a, long b) { long q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
static int is_leap(long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

/* Days since 1970-01-01 for a proleptic Gregorian date (m = 1..12). */
static long days_from_civil(long y, int m, int d)
{
    long era, yoe, doy, doe;
    y -= m <= 2;
    era = floor_div(y, 400);
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

struct tm *gmtime_r(const time_t *tp, struct tm *out)
{
    static const int cum[2][12] = {
        {0,31,59,90,120,151,181,212,243,273,304,334},
        {0,31,60,91,121,152,182,213,244,274,305,335} };
    long t, days, secs, z, era, doe, yoe, y, doy, mp;
    int m, d;
    if (!tp || !out) { errno = EINVAL; return NULL; }
    t = *tp;
    days = floor_div(t, 86400);
    secs = t - days * 86400;
    z = days + 719468;
    era = floor_div(z, 146097);
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    if (m <= 2) y++;
    if (y - 1900 > INT_MAX || y - 1900 < INT_MIN) { errno = EOVERFLOW; return NULL; }
    out->tm_year = (int)(y - 1900);
    out->tm_mon = m - 1;
    out->tm_mday = d;
    out->tm_hour = (int)(secs / 3600);
    out->tm_min = (int)(secs % 3600 / 60);
    out->tm_sec = (int)(secs % 60);
    out->tm_wday = (int)(((days % 7) + 11) % 7);       /* 1970-01-01 was a Thursday */
    out->tm_yday = cum[is_leap(y)][m - 1] + d - 1;
    out->tm_isdst = 0;
    out->tm_gmtoff = 0;
    out->tm_zone = "UTC";
    return out;
}

struct tm *gmtime(const time_t *t)
{
    static struct tm result;               /* not reentrant, as specified */
    return gmtime_r(t, &result);
}

time_t timegm(struct tm *tm)
{
    long y, days, total;
    long mon;
    if (!tm) { errno = EINVAL; return (time_t)-1; }
    y = (long)tm->tm_year + 1900;
    mon = tm->tm_mon;
    y += floor_div(mon, 12);
    mon -= floor_div(mon, 12) * 12;
    days = days_from_civil(y, (int)mon + 1, 1);
    if (__builtin_add_overflow(days, (long)tm->tm_mday - 1, &days) ||
        __builtin_mul_overflow(days, 86400L, &total) ||
        __builtin_add_overflow(total, (long)tm->tm_hour * 3600 + (long)tm->tm_min * 60 + tm->tm_sec, &total)) {
        errno = EOVERFLOW;
        return (time_t)-1;
    }
    { time_t t = total; if (!gmtime_r(&t, tm)) return (time_t)-1; }   /* normalise fields */
    return total;
}

time_t mktime(struct tm *tm) { return timegm(tm); }

/* ---- local time == UTC -------------------------------------------------- */

char *tzname[2] = { (char *)"UTC", (char *)"UTC" };
long timezone;
int daylight;
void tzset(void) {}

struct tm *localtime_r(const time_t *restrict t, struct tm *restrict out) { return gmtime_r(t, out); }
struct tm *localtime(const time_t *t)
{
    static struct tm result;
    return gmtime_r(t, &result);
}

static const char *const wday_name[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const mon_name[12] = { "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December" };

/* "Sun Sep 16 01:03:52 1973\n", exactly 26 bytes with the NUL for 4-digit years */
char *asctime_r(const struct tm *restrict tm, char *restrict buf)
{
    int n = snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
                     tm->tm_wday >= 0 && tm->tm_wday < 7 ? wday_name[tm->tm_wday] : "???",
                     tm->tm_mon >= 0 && tm->tm_mon < 12 ? mon_name[tm->tm_mon] : "???",
                     tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, 1900 + tm->tm_year);
    if (n < 0 || n >= 26) { errno = EOVERFLOW; return NULL; }
    return buf;
}
char *asctime(const struct tm *tm)
{
    static char buf[26];
    return asctime_r(tm, buf);
}
char *ctime_r(const time_t *t, char *buf)
{
    struct tm tm;
    return gmtime_r(t, &tm) ? asctime_r(&tm, buf) : NULL;
}
char *ctime(const time_t *t)
{
    static char buf[26];
    return ctime_r(t, buf);
}

/* ---- strftime ------------------------------------------------------------ */

/* ISO 8601 week-based year and week number. */
static int iso_week(const struct tm *tm, int *year)
{
    int yday = tm->tm_yday, wday = (tm->tm_wday + 6) % 7;     /* Monday = 0 */
    int y = tm->tm_year + 1900;
    int week = (yday - wday + 10) / 7;
    if (week < 1) {
        int py = y - 1;
        int pdays = ((py % 4 == 0 && py % 100 != 0) || py % 400 == 0) ? 366 : 365;
        week = (yday + pdays - wday + 10) / 7;
        y = py;
    } else {
        int days = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365;
        if (yday - wday + 3 >= days) { week = 1; y++; }       /* belongs to next year's week 1 */
    }
    *year = y;
    return week;
}

typedef struct { char *p; size_t left; int full; } sbuf;
static void put(sbuf *b, const char *s, size_t n)
{
    if (b->full) return;
    if (n >= b->left) { b->full = 1; return; }
    memcpy(b->p, s, n);
    b->p += n; b->left -= n;
}
static void putf(sbuf *b, const char *fmt, long v)
{
    char t[32];
    int n = snprintf(t, sizeof t, fmt, v);
    put(b, t, (size_t)n);
}

static void fmt_one(sbuf *b, char c, const struct tm *tm, int pad_none);

static void fmt_str(sbuf *b, const char *f, const struct tm *tm)
{
    for (; *f; f++) {
        int pad_none = 0;
        if (*f != '%') { put(b, f, 1); continue; }
        f++;
        while (*f == 'E' || *f == 'O' || *f == '-' || *f == '_' || *f == '0' || *f == '^' || *f == '#') {
            if (*f == '-') pad_none = 1;     /* GNU: no padding */
            f++;
        }
        if (!*f) { put(b, "%", 1); break; }
        fmt_one(b, *f, tm, pad_none);
    }
}

static void fmt_one(sbuf *b, char c, const struct tm *tm, int pad_none)
{
    int y = tm->tm_year + 1900, iy, h12 = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12;
    const char *d2 = pad_none ? "%ld" : "%02ld";
    switch (c) {
    case 'a': put(b, tm->tm_wday >= 0 && tm->tm_wday < 7 ? wday_name[tm->tm_wday] : "?", 3); break;
    case 'A': { const char *w = tm->tm_wday >= 0 && tm->tm_wday < 7 ? wday_name[tm->tm_wday] : "?"; put(b, w, strlen(w)); } break;
    case 'b': case 'h': put(b, tm->tm_mon >= 0 && tm->tm_mon < 12 ? mon_name[tm->tm_mon] : "?", 3); break;
    case 'B': { const char *m = tm->tm_mon >= 0 && tm->tm_mon < 12 ? mon_name[tm->tm_mon] : "?"; put(b, m, strlen(m)); } break;
    case 'c': fmt_str(b, "%a %b %e %H:%M:%S %Y", tm); break;
    case 'C': putf(b, d2, (long)(y / 100 - (y % 100 < 0))); break;
    case 'd': putf(b, d2, tm->tm_mday); break;
    case 'D': fmt_str(b, "%m/%d/%y", tm); break;
    case 'e': putf(b, pad_none ? "%ld" : "%2ld", tm->tm_mday); break;
    case 'F': fmt_str(b, "%Y-%m-%d", tm); break;
    case 'g': iso_week(tm, &iy); putf(b, d2, (long)(((iy % 100) + 100) % 100)); break;
    case 'G': iso_week(tm, &iy); putf(b, "%ld", iy); break;
    case 'H': putf(b, d2, tm->tm_hour); break;
    case 'I': putf(b, d2, h12); break;
    case 'j': putf(b, pad_none ? "%ld" : "%03ld", tm->tm_yday + 1); break;
    case 'k': putf(b, "%2ld", tm->tm_hour); break;
    case 'l': putf(b, "%2ld", h12); break;
    case 'm': putf(b, d2, tm->tm_mon + 1); break;
    case 'M': putf(b, d2, tm->tm_min); break;
    case 'n': put(b, "\n", 1); break;
    case 'p': put(b, tm->tm_hour < 12 ? "AM" : "PM", 2); break;
    case 'P': put(b, tm->tm_hour < 12 ? "am" : "pm", 2); break;
    case 'r': fmt_str(b, "%I:%M:%S %p", tm); break;
    case 'R': fmt_str(b, "%H:%M", tm); break;
    case 's': {
        struct tm copy = *tm;
        putf(b, "%ld", (long)timegm(&copy));
        break;
    }
    case 'S': putf(b, d2, tm->tm_sec); break;
    case 't': put(b, "\t", 1); break;
    case 'T': fmt_str(b, "%H:%M:%S", tm); break;
    case 'u': putf(b, "%ld", tm->tm_wday == 0 ? 7 : tm->tm_wday); break;
    case 'U': putf(b, d2, (tm->tm_yday + 7 - tm->tm_wday) / 7); break;
    case 'V': putf(b, d2, iso_week(tm, &iy)); break;
    case 'w': putf(b, "%ld", tm->tm_wday); break;
    case 'W': putf(b, d2, (tm->tm_yday + 7 - (tm->tm_wday + 6) % 7) / 7); break;
    case 'x': fmt_str(b, "%m/%d/%y", tm); break;
    case 'X': fmt_str(b, "%H:%M:%S", tm); break;
    case 'y': putf(b, d2, (long)(((y % 100) + 100) % 100)); break;
    case 'Y': putf(b, "%ld", y); break;
    case 'z': {
        long off = tm->tm_gmtoff, a = off < 0 ? -off : off;
        char t[16];
        snprintf(t, sizeof t, "%c%02ld%02ld", off < 0 ? '-' : '+', a / 3600, a % 3600 / 60);
        put(b, t, strlen(t));
        break;
    }
    case 'Z': { const char *z = tm->tm_zone ? tm->tm_zone : "UTC"; put(b, z, strlen(z)); } break;
    case '%': put(b, "%", 1); break;
    default: { char t[2] = { '%', c }; put(b, t, 2); } break;
    }
}

size_t strftime(char *restrict s, size_t max, const char *restrict fmt, const struct tm *restrict tm)
{
    sbuf b = { s, max, 0 };
    if (max == 0) return 0;
    fmt_str(&b, fmt, tm);
    if (b.full) { s[0] = 0; return 0; }
    *b.p = 0;
    return (size_t)(b.p - s);
}
