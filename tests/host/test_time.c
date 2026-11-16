/* The calendar: gmtime/timegm/strftime/asctime against glibc on random
 * instants from year -2000 to 10000. */
#include <time.h>
#include <string.h>
#include <stdint.h>
#include "harness.h"
#include "oracle.h"

static uint64_t rng = 0xabcdef1234567ULL;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

void test_time(void)
{
    static const char *const fmts[] = { "%Y-%m-%d %H:%M:%S", "%a %A %b %B %h", "%c|%x|%X|%D|%F|%T|%R|%r",
        "%y %e %j %u %w %U %W", "%G-W%V-%u %g", "%I %l %k %p %P", "%n%t%%", "%-d/%-m %-j %-H", "%z",
        "%Ey %OH %Ec" };
    char a[256], b[256];
    int k, bad = 0;
    for (k = 0; k < 30000; k++) {
        long t;
        struct tm tm;
        int f[9];
        size_t i;
        switch (k % 3) {
        case 0: t = (long)(next() % 400000000000ULL) - 125000000000L; break;   /* years -2000..10000 */
        case 1: t = (long)(next() % 4000000000ULL); break;
        default: t = (long)(next() % 60) * 86400 * 365 + (long)(next() % 20) * 86400 - 3 * 86400; break;
        }
        if (!gmtime_r(&t, &tm) || !oracle_gmtime_fields(t, f)) { bad++; continue; }
        if (tm.tm_sec != f[0] || tm.tm_min != f[1] || tm.tm_hour != f[2] || tm.tm_mday != f[3] ||
            tm.tm_mon != f[4] || tm.tm_year != f[5] || tm.tm_wday != f[6] || tm.tm_yday != f[7]) {
            if (bad++ < 5) th_note("  gmtime(%ld) differs\n", t);
        }
        { struct tm c = tm; if (timegm(&c) != t) { if (bad++ < 5) th_note("  timegm(gmtime(%ld)) differs\n", t); } }
        for (i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
            size_t n1 = strftime(a, sizeof a, fmts[i], &tm), n2 = oracle_strftime(b, sizeof b, fmts[i], t);
            if (n1 != n2 || strcmp(a, b) != 0) {
                if (bad++ < 8) th_note("  strftime(\"%s\", %ld): ours \"%s\" glibc \"%s\"\n", fmts[i], t, a, b);
            }
        }
        if (tm.tm_year >= -900 && tm.tm_year < 8100) {        /* asctime is only defined for 4-digit years */
            char *x = asctime_r(&tm, a), *y = oracle_ctime(t, b);
            if (!x || strcmp(x, y) != 0) { if (bad++ < 5) th_note("  asctime(%ld): ours %s glibc %s", t, x ? x : "NULL\n", y); }
        }
    }
    CHECK_EQ(bad, 0);
    { struct tm tm; time_t t = 0; gmtime_r(&t, &tm);
      CHECK_EQ(strftime(a, 5, "%Y-%m-%d", &tm), 0);           /* too small: 0 */
      CHECK_EQ(strftime(a, sizeof a, "%Z", &tm), 3); CHECK_STR(a, "UTC");
      CHECK(localtime_r(&t, &tm) && tm.tm_year == 70);
      CHECK_STR(ctime(&t), "Thu Jan  1 00:00:00 1970\n");
      /* %C is two digits, as POSIX says ([00,99]); glibc prints "6" for year 650 */
      CHECK(strftime(a, sizeof a, "%C", &tm) == 2 && strcmp(a, "19") == 0);
      tm.tm_year = 650 - 1900; strftime(a, sizeof a, "%C", &tm); CHECK_STR(a, "06"); }
}
