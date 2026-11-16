/* The host C library as a reference ("oracle") for libNovaC's tests.
 * Compiled with the HOST's headers, like harness.c, and called from test
 * TUs that only see libNovaC's headers, so every signature here uses plain
 * types. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "oracle.h"

int oracle_snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

double oracle_strtod(const char *s, char **end, int *err)
{
    double v;
    errno = 0;
    v = strtod(s, end);
    *err = errno;
    return v;
}

float oracle_strtof(const char *s, char **end, int *err)
{
    float v;
    errno = 0;
    v = strtof(s, end);
    *err = errno;
    return v;
}

/* Exact decimal of a long double; the long double format holds the exact
 * midpoint between two adjacent doubles. */
int oracle_ld_exact(char *buf, size_t n, long double v)
{
    return snprintf(buf, n, "%.1100Le", v);
}

long double oracle_ld_mid(double a, double b) { return ((long double)a + (long double)b) / 2; }

int oracle_sscanf_ii(const char *s, const char *fmt, int *a, int *b) { return sscanf(s, fmt, a, b); }
