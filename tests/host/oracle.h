#ifndef NOVAC_TEST_ORACLE_H
#define NOVAC_TEST_ORACLE_H
#include <stddef.h>
int    oracle_snprintf(char *buf, size_t n, const char *fmt, ...);
double oracle_strtod(const char *s, char **end, int *err);
float  oracle_strtof(const char *s, char **end, int *err);
int    oracle_ld_exact(char *buf, size_t n, long double v);
long double oracle_ld_mid(double a, double b);
#endif
