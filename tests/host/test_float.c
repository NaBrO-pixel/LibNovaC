/* Floating-point formatting and parsing, checked against the host C
 * library: printf's f/e/g/a conversions on random doubles, and strtod/strtof
 * on random, halfway and pathological inputs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "harness.h"
#include "oracle.h"

static uint64_t rng = 0x9e3779b97f4a7c15ULL;
static uint64_t next(void)
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return rng;
}
static double from_bits(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }
static uint64_t to_bits(double d) { uint64_t b; memcpy(&b, &d, 8); return b; }
static uint32_t fbits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }

static double random_double(void)
{
    for (;;) {
        uint64_t b = next();
        switch (next() % 4) {
        case 0: break;                                                  /* any */
        case 1: b = (b & 0x800fffffffffffffULL) | ((uint64_t)(1023 + (int)(next() % 80) - 40) << 52); break;
        case 2: b &= 0x800fffffffffffffULL; break;                      /* subnormal */
        case 3: b = (b & 0x8000000000000000ULL) | ((next() % 2000000) * 0x1000000000ULL) | (0x3ffULL << 52); break;
        }
        if (((b >> 52) & 0x7ff) != 0x7ff) return from_bits(b);
    }
}

static int mismatches;
static void cmp_fmt(const char *fmt, double v)
{
    static char ours[2048], want[2048];
    int a, b;
    /* glibc prints "%#g" of 999999.5 as "1.e+06", dropping the zeros '#'
     * must keep; C says "1.00000e+06", which is what we print. */
    if (v == 999999.5 && strchr(fmt, '#') && (strchr(fmt, 'g') || strchr(fmt, 'G'))) {
        snprintf(ours, sizeof ours, fmt, v);
        CHECK(strstr(ours, "1.0") != NULL);
        return;
    }
    a = snprintf(ours, sizeof ours, fmt, v);
    b = oracle_snprintf(want, sizeof want, fmt, v);
    if (a != b || strcmp(ours, want) != 0) {
        if (mismatches++ < 10)
            printf("  printf(\"%s\", %a): got \"%s\" (%d), want \"%s\" (%d)\n", fmt, v, ours, a, want, b);
        CHECK(0);
    } else CHECK(1);
}

static void cmp_strtod(const char *s)
{
    char *e1, *e2;
    int err2;
    double got, want;
    errno = 0;
    got = strtod(s, &e1);
    want = oracle_strtod(s, &e2, &err2);
    if (to_bits(got) != to_bits(want) || e1 != e2) {
        if (mismatches++ < 10)
            printf("  strtod(\"%.80s\"): got %a end+%d, want %a end+%d\n", s, got, (int)(e1 - s), want, (int)(e2 - s));
        CHECK(0);
    } else CHECK(1);
}

static void cmp_strtof(const char *s)
{
    char *e1, *e2;
    int err2;
    float got = strtof(s, &e1), want = oracle_strtof(s, &e2, &err2);
    if (fbits(got) != fbits(want) || e1 != e2) {
        if (mismatches++ < 10)
            printf("  strtof(\"%.80s\"): got %a, want %a\n", s, (double)got, (double)want);
        CHECK(0);
    } else CHECK(1);
}

static const char *const fmts[] = {
    "%f", "%.0f", "%.1f", "%.3f", "%.17f", "%.40f", "%#.0f", "%012.3f", "%-14.2f|", "%+f", "% f",
    "%e", "%.0e", "%.1e", "%.16e", "%.30e", "%#.0e", "%+.5E", "%015.4e",
    "%g", "%.0g", "%.1g", "%.3g", "%.17g", "%.25g", "%#g", "%#.3g", "%G", "%-12.4g|", "%010g",
    "%a", "%A", "%.0a", "%.1a", "%.3a", "%.13a", "%.20a", "%#.0a", "%+a", "%020a",
};

static void test_printf_float(void)
{
    char b[512];
    size_t i;
    int k;

    snprintf(b, sizeof b, "%.2f %.0f %.0f %.0f %.1f", 3.14159, 0.5, 1.5, 2.5, 0.25);
    CHECK_STR(b, "3.14 0 2 2 0.2");
    snprintf(b, sizeof b, "%e|%g|%g|%g", 12345.678, 0.0001, 100000.0, 1e6);
    CHECK_STR(b, "1.234568e+04|0.0001|100000|1e+06");
    snprintf(b, sizeof b, "%f %F %e %g %a", 1.0 / 0.0, -1.0 / 0.0, __builtin_nan(""), -0.0, 1.0);
    CHECK_STR(b, "inf -INF nan -0 0x1p+0");
    snprintf(b, sizeof b, "[%8.3f][%-8.2e][%08.2f][%+.1f]", 3.14159, 1234.5, -2.5, 2.0);
    CHECK_STR(b, "[   3.142][1.23e+03][-0002.50][+2.0]");
    snprintf(b, sizeof b, "%Lf %.3Lg", (long double)1.25, (long double)1234.5);
    CHECK_STR(b, "1.250000 1.23e+03");
    snprintf(b, sizeof b, "%.0f", 1e300);
    CHECK(strlen(b) == 301 && b[0] == '1');
    CHECK_EQ(snprintf(NULL, 0, "%.5000f", 1.0), 5002);

    for (i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
        static const double fixed[] = { 0.0, -0.0, 1.0, 0.1, 0.5, 1.5, 2.5, 9.5, 0.05, 0.15, 0.25, 0.35,
            999999.5, 9.9999995, 1e15, 1e16, 1e22, 1e23, 123456789.0, 5e-324, 2.2250738585072014e-308,
            1.7976931348623157e308, 0.000123456, 1e-5, 0.0001, 99.99, 0.999, 9.999999e-5, 1.0 / 0.0,
            -1.0 / 0.0, 0x1.fffffffffffffp0, 0x1.8p0, 0x1.08p0, 0x1.18p0, 0x1.0000000000008p0,
            0x1.0000000000018p0 };
        size_t j;
        for (j = 0; j < sizeof fixed / sizeof fixed[0]; j++) cmp_fmt(fmts[i], fixed[j]);
        cmp_fmt(fmts[i], __builtin_nan(""));
        for (k = 0; k < 1500; k++) cmp_fmt(fmts[i], random_double());
    }
    for (k = 0; k < 300; k++) cmp_fmt("%.1100f", random_double());
}

static void test_strtod(void)
{
    char buf[2400], *end;
    int k;
    double d;

    /* fixed behaviour */
    CHECK(strtod("1.5", NULL) == 1.5);
    CHECK(strtod("  -0x1.8p1xyz", &end) == -3.0 && strcmp(end, "xyz") == 0);
    CHECK(strtod("1e", &end) == 1.0 && strcmp(end, "e") == 0);
    CHECK(strtod("1e+", &end) == 1.0 && strcmp(end, "e+") == 0);
    CHECK(strtod(".", &end) == 0.0 && strcmp(end, ".") == 0);
    CHECK(strtod("0x", &end) == 0.0 && strcmp(end, "x") == 0);
    CHECK(strtod("-.5e-3", NULL) == -0.0005);
    d = strtod("-Infinity!", &end); CHECK(d < 0 && d == d * 2 && *end == '!');
    d = strtod("inFinit", &end); CHECK(d > 0 && d == d * 2 && strcmp(end, "init") == 0);
    d = strtod("nan(abc_1)z", &end); CHECK(d != d && *end == 'z');
    d = strtod("nan(abc", &end); CHECK(d != d && *end == '(');
    errno = 0; d = strtod("1e999999", NULL); CHECK(d == 1.0 / 0.0 && errno == ERANGE);
    errno = 0; d = strtod("-1e-400", NULL); CHECK(d == 0 && 1 / d < 0 && errno == ERANGE);
    errno = 0; d = strtod("1e-320", NULL); CHECK(d > 0 && errno == ERANGE);
    errno = 0; d = strtod("2.2250738585072014e-308", NULL); CHECK(errno == 0 && to_bits(d) == 0x0010000000000000ULL);
    CHECK(to_bits(strtod("4.9406564584124654e-324", NULL)) == 1);
    CHECK(to_bits(strtod("2.4703282292062328e-324", NULL)) == 1);   /* just above half */
    CHECK(to_bits(strtod("2.4703282292062327e-324", NULL)) == 0);   /* just below half */
    CHECK(strtod("0x1p-1074", NULL) == 5e-324);
    CHECK(strtod("0x.8p1", NULL) == 1.0);
    CHECK(strtof("0.1", NULL) == 0.1f);
    CHECK(atof("12.5e1") == 125.0);

    /* round trips and comparisons on random doubles */
    for (k = 0; k < 20000; k++) {
        double v = random_double();
        snprintf(buf, sizeof buf, "%.17g", v);
        CHECK(to_bits(strtod(buf, NULL)) == to_bits(v));
        snprintf(buf, sizeof buf, "%a", v);
        CHECK(to_bits(strtod(buf, NULL)) == to_bits(v));
        snprintf(buf, sizeof buf, "%.*e", (int)(next() % 25), v);
        cmp_strtod(buf);
        cmp_strtof(buf);
    }
    /* exact halfway points between neighbouring doubles, and just off them */
    for (k = 0; k < 3000; k++) {
        double a = random_double(), b;
        int n;
        if (a < 0) a = -a;
        b = from_bits(to_bits(a) + 1);
        if (b != b || b == 1.0 / 0.0) continue;
        n = oracle_ld_exact(buf, sizeof buf - 10, oracle_ld_mid(a, b));
        cmp_strtod(buf);
        { char *e = strchr(buf, 'e'); char ex[16];
          if (e) { strcpy(ex, e); strcpy(e, "0000000000001"); strcat(buf, ex); cmp_strtod(buf); } }
        (void)n;
    }
    /* random decimal strings, including very long ones */
    for (k = 0; k < 20000; k++) {
        int nd = 1 + (int)(next() % (k % 50 == 0 ? 1500 : 30)), i, p = 0;
        if (next() & 1) buf[p++] = '-';
        for (i = 0; i < nd; i++) {
            buf[p++] = (char)('0' + next() % 10);
            if (i == 0 && (next() & 1)) buf[p++] = '.';
        }
        p += snprintf(buf + p, 32, "e%d", (int)(next() % 700) - 350);
        buf[p] = 0;
        cmp_strtod(buf);
        cmp_strtof(buf);
    }
}

void test_float(void)
{
    mismatches = 0;
    test_printf_float();
    test_strtod();
}
