/* Fixed-capacity big integers for the floating-point conversions. */
#include <string.h>
#include "../internal/bigint.h"

static void trim(novac_big *a)
{
    while (a->n > 0 && a->d[a->n - 1] == 0) a->n--;
}

void novac_big_set_u64(novac_big *a, uint64_t v)
{
    a->d[0] = (uint32_t)v;
    a->d[1] = (uint32_t)(v >> 32);
    a->n = 2;
    trim(a);
}

void novac_big_copy(novac_big *dst, const novac_big *src)
{
    dst->n = src->n;
    memcpy(dst->d, src->d, (size_t)src->n * sizeof src->d[0]);
}

void novac_big_mul_small(novac_big *a, uint32_t m)
{
    uint64_t carry = 0;
    int i;
    for (i = 0; i < a->n; i++) {
        uint64_t t = (uint64_t)a->d[i] * m + carry;
        a->d[i] = (uint32_t)t;
        carry = t >> 32;
    }
    if (carry && a->n < NOVAC_BIG_LIMBS) a->d[a->n++] = (uint32_t)carry;
    trim(a);
}

void novac_big_add_small(novac_big *a, uint32_t v)
{
    int i = 0;
    uint64_t carry = v;
    while (carry) {
        if (i == a->n) {
            if (a->n == NOVAC_BIG_LIMBS) return;
            a->d[a->n++] = 0;
        }
        carry += a->d[i];
        a->d[i++] = (uint32_t)carry;
        carry >>= 32;
    }
}

void novac_big_mul_pow5(novac_big *a, unsigned k)
{
    static const uint32_t p5[] = { 1, 5, 25, 125, 625, 3125, 15625, 78125, 390625,
        1953125, 9765625, 48828125, 244140625, 1220703125 };
    while (k >= 13) { novac_big_mul_small(a, p5[13]); k -= 13; }
    if (k) novac_big_mul_small(a, p5[k]);
}

void novac_big_mul_pow10(novac_big *a, unsigned k)
{
    novac_big_mul_pow5(a, k);
    novac_big_shl(a, k);
}

void novac_big_shl(novac_big *a, unsigned bits)
{
    unsigned limbs = bits / 32, sh = bits % 32;
    int i, n;
    if (a->n == 0 || bits == 0) return;
    n = a->n + (int)limbs + 1;
    if (n > NOVAC_BIG_LIMBS) n = NOVAC_BIG_LIMBS;
    for (i = n - 1; i >= 0; i--) {
        int src = i - (int)limbs;
        uint32_t hi = (src >= 0 && src < a->n) ? a->d[src] : 0;
        uint32_t lo = (src - 1 >= 0 && src - 1 < a->n) ? a->d[src - 1] : 0;
        a->d[i] = sh ? (hi << sh) | (lo >> (32 - sh)) : hi;
    }
    a->n = n;
    trim(a);
}

void novac_big_shr1(novac_big *a)
{
    int i;
    for (i = 0; i < a->n; i++)
        a->d[i] = (a->d[i] >> 1) | (i + 1 < a->n ? a->d[i + 1] << 31 : 0);
    trim(a);
}

int novac_big_cmp(const novac_big *a, const novac_big *b)
{
    int i;
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] < b->d[i] ? -1 : 1;
    return 0;
}

void novac_big_sub(novac_big *a, const novac_big *b)
{
    int64_t borrow = 0;
    int i;
    for (i = 0; i < a->n; i++) {
        int64_t t = (int64_t)a->d[i] - (i < b->n ? (int64_t)b->d[i] : 0) - borrow;
        borrow = t < 0;
        a->d[i] = (uint32_t)(t + (borrow ? ((int64_t)1 << 32) : 0));
    }
    trim(a);
}

uint32_t novac_big_divmod_small(novac_big *a, uint32_t d)
{
    uint64_t rem = 0;
    int i;
    for (i = a->n - 1; i >= 0; i--) {
        uint64_t cur = (rem << 32) | a->d[i];
        a->d[i] = (uint32_t)(cur / d);
        rem = cur % d;
    }
    trim(a);
    return (uint32_t)rem;
}

unsigned novac_big_bitlen(const novac_big *a)
{
    uint32_t top;
    unsigned bits;
    if (a->n == 0) return 0;
    top = a->d[a->n - 1];
    bits = 0;
    while (top) { bits++; top >>= 1; }
    return (unsigned)(a->n - 1) * 32 + bits;
}

static unsigned bit_at(const novac_big *a, unsigned i)
{
    unsigned limb = i / 32;
    if ((int)limb >= a->n) return 0;
    return (a->d[limb] >> (i % 32)) & 1u;
}

uint64_t novac_big_bits64(const novac_big *a, unsigned lo, int *sticky_below)
{
    uint64_t r = 0;
    unsigned i;
    for (i = 0; i < 64; i++) r |= (uint64_t)bit_at(a, lo + i) << i;
    if (sticky_below) {
        int s = 0;
        unsigned full = lo / 32, j;
        for (j = 0; j < full && (int)j < a->n && !s; j++) if (a->d[j]) s = 1;
        if (!s && (int)full < a->n && (lo % 32) && (a->d[full] & ((1u << (lo % 32)) - 1))) s = 1;
        *sticky_below = s;
    }
    return r;
}
