/* Fixed-capacity unsigned big integers, used for exact binary <-> decimal
 * conversion (printf's %e/%f/%g and strtod). Little-endian 32-bit limbs.
 * The capacity covers the largest value either conversion can build
 * (about 3,900 bits); callers bound their inputs so it is never exceeded. */
#ifndef NOVAC_INTERNAL_BIGINT_H
#define NOVAC_INTERNAL_BIGINT_H
#include <stdint.h>

#define NOVAC_BIG_LIMBS 160

typedef struct {
    int n;                              /* limbs in use; 0 means the value 0 */
    uint32_t d[NOVAC_BIG_LIMBS];
} novac_big;

void     novac_big_set_u64(novac_big *a, uint64_t v);
void     novac_big_copy(novac_big *dst, const novac_big *src);
void     novac_big_mul_small(novac_big *a, uint32_t m);
void     novac_big_add_small(novac_big *a, uint32_t v);
void     novac_big_mul_pow5(novac_big *a, unsigned k);
void     novac_big_mul_pow10(novac_big *a, unsigned k);
void     novac_big_shl(novac_big *a, unsigned bits);
void     novac_big_shr1(novac_big *a);
int      novac_big_cmp(const novac_big *a, const novac_big *b);
void     novac_big_sub(novac_big *a, const novac_big *b);       /* requires a >= b */
uint32_t novac_big_divmod_small(novac_big *a, uint32_t d);      /* a /= d, returns a % d */
unsigned novac_big_bitlen(const novac_big *a);
/* The 64 bits starting at bit `lo` (bits above the value read as 0), and
 * whether any bit below `lo` is set. */
uint64_t novac_big_bits64(const novac_big *a, unsigned lo, int *sticky_below);

#endif
