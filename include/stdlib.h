#ifndef NOVAC_STDLIB_H
#define NOVAC_STDLIB_H
#include <stddef.h>
#include <novac/defs.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     32767

typedef struct { int       quot, rem; } div_t;
typedef struct { long      quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

NOVAC_BEGIN_DECLS
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);
/* align must be a power of two (and, for posix_memalign, a multiple of
 * sizeof(void *)). The memory is released with free(). */
void *aligned_alloc(size_t align, size_t size);
int   posix_memalign(void **out, size_t align, size_t size);
void *memalign(size_t align, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
size_t malloc_usable_size(void *ptr);

_Noreturn void abort(void);
_Noreturn void exit(int status);
_Noreturn void _Exit(int status);
int   atexit(void (*fn)(void));
char *getenv(const char *name);
int   setenv(const char *name, const char *value, int overwrite);
int   unsetenv(const char *name);

int       atoi(const char *s);
long      atol(const char *s);
long long atoll(const char *s);
long               strtol(const char *restrict s, char **restrict end, int base);
long long          strtoll(const char *restrict s, char **restrict end, int base);
unsigned long      strtoul(const char *restrict s, char **restrict end, int base);
unsigned long long strtoull(const char *restrict s, char **restrict end, int base);
/* Correctly rounded (ties to even); decimal, hex ("0x1.8p3"), inf, nan.
 * strtold converts at double precision. */
double      strtod(const char *restrict s, char **restrict end);
float       strtof(const char *restrict s, char **restrict end);
long double strtold(const char *restrict s, char **restrict end);
double      atof(const char *s);

int       abs(int x);
long      labs(long x);
long long llabs(long long x);
div_t     div(int n, int d);
ldiv_t    ldiv(long n, long d);
lldiv_t   lldiv(long long n, long long d);

void  qsort(void *base, size_t nmemb, size_t size,
            int (*cmp)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*cmp)(const void *, const void *));
/* GNU argument order: the context is the comparator's third argument. */
void  qsort_r(void *base, size_t nmemb, size_t size,
              int (*cmp)(const void *, const void *, void *), void *arg);

/* Linear congruential generator. NOT suitable for security purposes. */
int   rand(void);
void  srand(unsigned int seed);
int   rand_r(unsigned int *seed);
long  random(void);
void  srandom(unsigned int seed);
/* POSIX 48-bit generators: same sequences as any other C library. */
double drand48(void);
double erand48(unsigned short x[3]);
long   lrand48(void);
long   nrand48(unsigned short x[3]);
long   mrand48(void);
long   jrand48(unsigned short x[3]);
void   srand48(long seed);
unsigned short *seed48(unsigned short seed[3]);
void   lcong48(unsigned short param[7]);
NOVAC_END_DECLS
#endif
