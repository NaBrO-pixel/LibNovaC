/* Binary -> exact decimal, and correctly rounded binary from a scaled
 * integer. Both are exact: no step loses information. */
#include <string.h>
#include "../internal/fp.h"
#include "../internal/bigint.h"

int novac__dtoa_exact(double x, char *digits, int *dp)
{
    uint64_t bits, m;
    int e, n = 0, i;
    novac_big b;
    uint32_t chunks[NOVAC_DTOA_MAX / 9 + 2];
    int nchunks = 0;
    char tmp[NOVAC_DTOA_MAX + 16];

    memcpy(&bits, &x, sizeof bits);
    m = bits & ((1ULL << 52) - 1);
    e = (int)((bits >> 52) & 0x7ff);
    if (e == 0) e = 1;                      /* subnormal */
    else m |= 1ULL << 52;
    e -= 1075;                              /* x = m * 2^e */
    while (m && !(m & 1)) { m >>= 1; e++; } /* smaller numbers, fewer limbs */

    novac_big_set_u64(&b, m);
    if (e >= 0) novac_big_shl(&b, (unsigned)e);
    else novac_big_mul_pow5(&b, (unsigned)-e);   /* x = b / 10^-e */

    while (b.n) chunks[nchunks++] = novac_big_divmod_small(&b, 1000000000u);
    for (i = nchunks - 1; i >= 0; i--) {
        uint32_t c = chunks[i];
        char nine[9];
        int k;
        for (k = 8; k >= 0; k--) { nine[k] = (char)('0' + c % 10); c /= 10; }
        if (i == nchunks - 1) {                 /* no leading zeros */
            k = 0;
            while (k < 8 && nine[k] == '0') k++;
            memcpy(tmp + n, nine + k, (size_t)(9 - k));
            n += 9 - k;
        } else {
            memcpy(tmp + n, nine, 9);
            n += 9;
        }
    }
    *dp = e >= 0 ? n : n + e;
    while (n > 0 && tmp[n - 1] == '0') n--;
    memcpy(digits, tmp, (size_t)n);
    return n;
}

/* Generic rounding to a binary format with `prec` significand bits,
 * minimum normal exponent `emin` and bias `bias`. Returns the encoding. */
static uint64_t round_bits(uint64_t m, int e, int sticky, int prec, int emin,
                           int bias, int *range)
{
    int E, p, shift, tiny;
    uint64_t kept, rem, half, maxbits;
    int lz = 0;

    while (!(m & (1ULL << 63))) { m <<= 1; lz++; }
    e -= lz;
    E = e + 63;                             /* value = 1.xxx * 2^E */
    maxbits = (uint64_t)(2 * bias + 1) << (prec - 1);   /* infinity */
    if (E > bias) { *range = 1; return maxbits; }
    tiny = E < emin;
    p = tiny ? prec - (emin - E) : prec;
    if (p < 0) { *range = 1; return 0; }
    if (p == 0) {                           /* below the smallest subnormal */
        *range = 1;
        return (m > (1ULL << 63) || sticky) ? 1 : 0;
    }
    shift = 64 - p;
    kept = m >> shift;
    rem = m & ((1ULL << shift) - 1);
    half = 1ULL << (shift - 1);
    if (rem > half || (rem == half && (sticky || (kept & 1)))) kept++;
    if (tiny && (rem || sticky)) *range = 1;
    if (tiny) return kept;                  /* a carry lands in the exponent field */
    kept += (uint64_t)(E + bias - 1) << (prec - 1);
    if (kept >= maxbits) { *range = 1; return maxbits; }
    return kept;
}

double novac__round_double(uint64_t m, int e, int sticky, int *range)
{
    uint64_t bits = round_bits(m, e, sticky, 53, -1022, 1023, range);
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

float novac__round_float(uint64_t m, int e, int sticky, int *range)
{
    uint32_t bits = (uint32_t)round_bits(m, e, sticky, 24, -126, 127, range);
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}
