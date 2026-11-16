/* stdlib additions of 0.3: aligned allocation, the 48-bit generators,
 * qsort_r, strtoimax, locale, fnmatch. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <inttypes.h>
#include <locale.h>
#include <fnmatch.h>
#include "harness.h"
#include "oracle.h"
#include "../../src/internal/heap.h"

static int cmp_mod(const void *a, const void *b, void *arg)
{
    int m = *(int *)arg, x = *(const int *)a % m, y = *(const int *)b % m;
    return (x > y) - (x < y);
}

static uint64_t rng = 88172645463325252ULL;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

void test_stdext(void)
{
    size_t al;
    int i, good = 1;
    void *ps[64];

    /* aligned allocation: every alignment, interleaved with frees */
    for (al = 1; al <= 65536; al <<= 1) {
        for (i = 0; i < 64; i++) {
            size_t n = 1 + (size_t)(next() % 3000);
            ps[i] = aligned_alloc(al, n);
            if (!ps[i] || ((uintptr_t)ps[i] & (al - 1))) good = 0;
            else { memset(ps[i], 0x77, n); if (malloc_usable_size(ps[i]) < n) good = 0; }
            if (i % 3 == 1) { free(ps[i - 1]); ps[i - 1] = NULL; }
        }
        for (i = 0; i < 64; i++) free(ps[i]);
        if (novac__heap_check() != 0) good = 0;
    }
    CHECK(good);
    { void *p = NULL;
      CHECK_EQ(posix_memalign(&p, 4096, 100), 0); CHECK(p && ((uintptr_t)p & 4095) == 0); free(p);
      CHECK_EQ(posix_memalign(&p, 3, 100), EINVAL);
      CHECK_EQ(posix_memalign(&p, 4, 100), EINVAL);           /* not a multiple of sizeof(void *) */
      errno = 0; CHECK(aligned_alloc(24, 10) == NULL && errno == EINVAL);
      p = memalign(64, 10); CHECK(p && ((uintptr_t)p & 63) == 0); free(p); }
    { int *a = reallocarray(NULL, 10, sizeof(int));
      CHECK(a != NULL); free(a);
      errno = 0; CHECK(reallocarray(NULL, SIZE_MAX / 2, 4) == NULL && errno == ENOMEM); }
    CHECK_EQ(novac__heap_check(), 0);

    /* drand48 family: the same sequence as glibc's */
    srand48(12345); oracle_srand48(12345);
    for (i = 0, good = 1; i < 1000; i++) {
        if (drand48() != oracle_drand48()) good = 0;
        if (lrand48() != oracle_lrand48()) good = 0;
        if (mrand48() != oracle_mrand48()) good = 0;
    }
    CHECK(good);
    { unsigned short s[3] = { 1, 2, 3 }; double a = erand48(s), b = erand48(s); CHECK(a != b && a >= 0 && a < 1); }
    { unsigned seed = 7; int a = rand_r(&seed), b = rand_r(&seed); CHECK(a != b && a >= 0 && a <= RAND_MAX); }
    srandom(1); { long a = random(); srandom(1); CHECK(random() == a && a >= 0); }

    { int v[] = { 13, 4, 25, 7, 31, 2 }, m = 10;
      qsort_r(v, 6, sizeof v[0], cmp_mod, &m);
      for (i = 1, good = 1; i < 6; i++) if (v[i - 1] % 10 > v[i] % 10) good = 0;
      CHECK(good); }

    { char *e;
      CHECK(strtoimax("-9223372036854775808", &e, 10) == INTMAX_MIN && *e == 0);
      CHECK(strtoumax("0xffffffffffffffff", NULL, 16) == UINTMAX_MAX);
      CHECK(imaxabs(-5) == 5 && imaxdiv(17, 5).rem == 2);
      { char b[64]; snprintf(b, sizeof b, "%" PRId64 " %" PRIx32 " %" PRIuMAX, (int64_t)-1, (uint32_t)255, (uintmax_t)7);
        CHECK_STR(b, "-1 ff 7"); } }

    { char *p = NULL;
      CHECK_EQ(asprintf(&p, "%s=%d %.2f", "x", 42, 2.5), 9); CHECK_STR(p, "x=42 2.50"); free(p);
      CHECK_EQ(asprintf(&p, "%s", ""), 0); CHECK_STR(p, ""); free(p);
      { char big[3000]; memset(big, 'q', sizeof big - 1); big[sizeof big - 1] = 0;
        CHECK_EQ(asprintf(&p, "<%s>", big), 3001); CHECK(p && p[0] == '<' && p[3000] == '>' && p[3001] == 0); free(p); } }

    CHECK_STR(setlocale(LC_ALL, ""), "C");
    CHECK_STR(setlocale(LC_NUMERIC, NULL), "C");
    CHECK(setlocale(LC_ALL, "en_US.UTF-8") == NULL);
    CHECK_STR(localeconv()->decimal_point, ".");

    /* fnmatch: fixed cases, then random patterns against glibc */
    CHECK_EQ(fnmatch("*.c", "main.c", 0), 0);
    CHECK_EQ(fnmatch("*.c", "main.h", 0), FNM_NOMATCH);
    CHECK_EQ(fnmatch("a?c", "abc", 0), 0);
    CHECK_EQ(fnmatch("[a-c]x", "bx", 0), 0);
    CHECK_EQ(fnmatch("[!a-c]x", "bx", 0), FNM_NOMATCH);
    CHECK_EQ(fnmatch("*", "a/b", FNM_PATHNAME), FNM_NOMATCH);
    CHECK_EQ(fnmatch("*/*", "a/b", FNM_PATHNAME), 0);
    CHECK_EQ(fnmatch("*", ".hidden", FNM_PERIOD), FNM_NOMATCH);
    CHECK_EQ(fnmatch(".*", ".hidden", FNM_PERIOD), 0);
    CHECK_EQ(fnmatch("a/*", "a/.x", FNM_PATHNAME | FNM_PERIOD), FNM_NOMATCH);
    CHECK_EQ(fnmatch("\\*", "*", 0), 0);
    CHECK_EQ(fnmatch("\\*", "a", 0), FNM_NOMATCH);
    CHECK_EQ(fnmatch("[[:digit:]]*", "7up", 0), 0);
    CHECK_EQ(fnmatch("ABC", "abc", FNM_CASEFOLD), 0);
    CHECK_EQ(fnmatch("a*", "a/b/c", FNM_PATHNAME | FNM_LEADING_DIR), 0);
    CHECK_EQ(fnmatch("[]]", "]", 0), 0);
    CHECK_EQ(fnmatch("[", "[", 0), 0);
    {
        static const char alpha[] = "ab./*?[]!-\\";
        static const char salpha[] = "ab./.";
        int k, bad = 0;
        for (k = 0; k < 40000; k++) {
            char p[12], s[12];
            int pl = (int)(next() % 9), sl = (int)(next() % 9), j, flags = (int)(next() % 8) & 7;
            for (j = 0; j < pl; j++) p[j] = alpha[next() % (sizeof alpha - 1)];
            for (j = 0; j < sl; j++) s[j] = salpha[next() % (sizeof salpha - 1)];
            p[pl] = s[sl] = 0;
            /* not compared: "[]" / "[!]" edge parses, and POSIX collating
             * symbols ("[.x.]", "[=x=]"), which libNovaC does not support */
            if (strstr(p, "[") && (strstr(p, "[!]") || strstr(p, "[]") || strstr(p, "[.") || strstr(p, "[="))) continue;
            if ((fnmatch(p, s, flags) == 0) != (oracle_fnmatch(p, s, flags) == 0)) {
                if (bad++ < 8) th_note("  fnmatch(\"%s\", \"%s\", %d): ours %d glibc %d\n", p, s, flags,
                                      fnmatch(p, s, flags), oracle_fnmatch(p, s, flags));
            }
        }
        CHECK_EQ(bad, 0);
    }
}
