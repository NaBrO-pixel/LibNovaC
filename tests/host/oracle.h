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
int    oracle_fnmatch(const char *p, const char *s, int flags);
double oracle_drand48(void);
long   oracle_lrand48(void);
long   oracle_mrand48(void);
void   oracle_srand48(long s);
int    oracle_inet_pton(int af, const char *s, void *dst);
const char *oracle_inet_ntop(int af, const void *src, char *dst, unsigned size);
int    oracle_inet_aton(const char *s, unsigned *out);
void   oracle_getopt_trace(int argc, char **argv, const char *opts, const void *longopts, char *out, size_t n);
#endif
