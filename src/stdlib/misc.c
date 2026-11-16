/* abs/div, qsort/bsearch, rand. */
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

/* abs(INT_MIN) is undefined in C; we return it unchanged (two's complement
 * wrap) instead of trapping. Documented in the README. */
int       abs(int x)         { return x < 0 ? (int)(0u - (unsigned)x) : x; }
long      labs(long x)       { return x < 0 ? (long)(0ul - (unsigned long)x) : x; }
long long llabs(long long x) { return x < 0 ? (long long)(0ull - (unsigned long long)x) : x; }

div_t   div(int n, int d)             { div_t r   = { n / d, n % d }; return r; }
ldiv_t  ldiv(long n, long d)          { ldiv_t r  = { n / d, n % d }; return r; }
lldiv_t lldiv(long long n, long long d){ lldiv_t r = { n / d, n % d }; return r; }

typedef int (*cmp_fn)(const void *, const void *);

static void swap_bytes(unsigned char *a, unsigned char *b, size_t size)
{
    while (size--) { unsigned char t = *a; *a++ = *b; *b++ = t; }
}

static void sift_down(unsigned char *base, size_t root, size_t count,
                      size_t size, cmp_fn cmp)
{
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= count) return;
        if (child + 1 < count &&
            cmp(base + child * size, base + (child + 1) * size) < 0)
            child++;
        if (cmp(base + root * size, base + child * size) >= 0) return;
        swap_bytes(base + root * size, base + child * size, size);
        root = child;
    }
}

/* Heapsort: O(n log n) worst case, in place, no recursion, no allocation
 * (so it is usable before the heap exists). Not stable. */
void qsort(void *base, size_t nmemb, size_t size, cmp_fn cmp)
{
    unsigned char *b = base;
    size_t i;
    if (nmemb < 2 || size == 0) return;
    if (!base || !cmp || nmemb > SIZE_MAX / 2 || nmemb > SIZE_MAX / size) {
        errno = EINVAL;
        return;
    }
    for (i = nmemb / 2; i-- > 0;) sift_down(b, i, nmemb, size, cmp);
    for (i = nmemb; i-- > 1;) {
        swap_bytes(b, b + i * size, size);
        sift_down(b, 0, i, size, cmp);
    }
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              cmp_fn cmp)
{
    const unsigned char *b = base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(key, b + mid * size);
        if (c == 0) return (void *)(b + mid * size);
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return NULL;
}

/* qsort_r: GNU argument order, the context last. A heapsort like qsort. */
typedef int (*cmp_r_fn)(const void *, const void *, void *);
static void sift_down_r(unsigned char *base, size_t root, size_t count, size_t size,
                        cmp_r_fn cmp, void *arg)
{
    for (;;) {
        size_t child = 2 * root + 1;
        if (child >= count) return;
        if (child + 1 < count && cmp(base + child * size, base + (child + 1) * size, arg) < 0) child++;
        if (cmp(base + root * size, base + child * size, arg) >= 0) return;
        swap_bytes(base + root * size, base + child * size, size);
        root = child;
    }
}
void qsort_r(void *base, size_t nmemb, size_t size, cmp_r_fn cmp, void *arg)
{
    unsigned char *b = base;
    size_t i;
    if (nmemb < 2 || size == 0) return;
    for (i = nmemb / 2; i-- > 0;) sift_down_r(b, i, nmemb, size, cmp, arg);
    for (i = nmemb; i-- > 1;) {
        swap_bytes(b, b + i * size, size);
        sift_down_r(b, 0, i, size, cmp, arg);
    }
}

static uint32_t rand_state = 1;
void srand(unsigned int seed) { rand_state = seed; }
int rand(void)
{
    rand_state = rand_state * 1103515245u + 12345u;
    return (int)((rand_state >> 16) & 0x7fff);
}

int rand_r(unsigned int *seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return (int)((*seed >> 16) & 0x7fff);
}

/* random/srandom: a 64-bit xorshift* generator, returning 31 bits. */
static uint64_t rnd_state = 0x853c49e6748fea9bULL;
void srandom(unsigned int seed) { rnd_state = (uint64_t)seed * 0x9e3779b97f4a7c15ULL + 1; }
long random(void)
{
    rnd_state ^= rnd_state >> 12; rnd_state ^= rnd_state << 25; rnd_state ^= rnd_state >> 27;
    return (long)((rnd_state * 0x2545F4914F6CDD1DULL) >> 33);
}

/* The POSIX 48-bit linear congruential family, exactly as specified:
 * X(n+1) = (a X(n) + c) mod 2^48, so results match every other libc. */
static unsigned short x48[3] = { 0x330e, 0xabcd, 0x1234 };
static unsigned short a48[3] = { 0xe66d, 0xdeec, 0x0005 };
static unsigned short c48 = 0xb;

static uint64_t get48(const unsigned short v[3])
{ return (uint64_t)v[0] | (uint64_t)v[1] << 16 | (uint64_t)v[2] << 32; }
static void put48(unsigned short v[3], uint64_t x)
{ v[0] = (unsigned short)x; v[1] = (unsigned short)(x >> 16); v[2] = (unsigned short)(x >> 32); }
static uint64_t step48(unsigned short x[3])
{
    uint64_t v = (get48(a48) * get48(x) + c48) & 0xffffffffffffULL;
    put48(x, v);
    return v;
}

double erand48(unsigned short x[3]) { return (double)step48(x) * 0x1p-48; }
double drand48(void)                 { return erand48(x48); }
long nrand48(unsigned short x[3])    { return (long)(step48(x) >> 17); }
long lrand48(void)                   { return nrand48(x48); }
long jrand48(unsigned short x[3])    { return (long)(int32_t)(uint32_t)(step48(x) >> 16); }
long mrand48(void)                   { return jrand48(x48); }
void srand48(long seed)
{
    x48[0] = 0x330e; x48[1] = (unsigned short)seed; x48[2] = (unsigned short)((unsigned long)seed >> 16);
    put48(a48, 0x5deece66dULL); c48 = 0xb;
}
unsigned short *seed48(unsigned short s[3])
{
    static unsigned short old[3];
    old[0] = x48[0]; old[1] = x48[1]; old[2] = x48[2];
    x48[0] = s[0]; x48[1] = s[1]; x48[2] = s[2];
    put48(a48, 0x5deece66dULL); c48 = 0xb;
    return old;
}
void lcong48(unsigned short p[7])
{
    x48[0] = p[0]; x48[1] = p[1]; x48[2] = p[2];
    a48[0] = p[3]; a48[1] = p[4]; a48[2] = p[5];
    c48 = p[6];
}
