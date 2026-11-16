#ifndef NOVAC_TEST_ORACLE_H
#define NOVAC_TEST_ORACLE_H
#include <stddef.h>
int    oracle_snprintf(char *buf, size_t n, const char *fmt, ...);
double oracle_strtod(const char *s, char **end, int *err);
float  oracle_strtof(const char *s, char **end, int *err);
int    oracle_ld_exact(char *buf, size_t n, long double v);
long double oracle_ld_mid(double a, double b);

double oracle_m1(int id, double x);
double oracle_m2(int id, double x, double y);
float  oracle_m1f(int id, float x);
double oracle_frexp(double x, int *e);
double oracle_remquo(double x, double y, int *q);
double oracle_scalbn(double x, int n);
long   oracle_lround(double x);
#endif
