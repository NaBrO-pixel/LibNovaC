/* Floating-point conversion internals shared by printf, strtod and scanf. */
#ifndef NOVAC_INTERNAL_FP_H
#define NOVAC_INTERNAL_FP_H
#include <stdint.h>

/* Room for the exact decimal expansion of any finite double (at most 767
 * significant digits) plus slack. */
#define NOVAC_DTOA_MAX 800

/* Exact decimal expansion of a finite x > 0: writes the significant digits
 * ('0'..'9', no leading or trailing zeros, not NUL-terminated) and sets *dp
 * so that x = 0.DIGITS * 10^dp. Returns the digit count. */
int novac__dtoa_exact(double x, char *digits, int *dp);

/* Rounds m * 2^e - plus an infinitesimal if `sticky` - to the nearest
 * double (ties to even), handling subnormals and overflow. *range is set
 * to 1 on overflow, or on an inexact result in the subnormal range. m != 0. */
double novac__round_double(uint64_t m, int e, int sticky, int *range);
float  novac__round_float(uint64_t m, int e, int sticky, int *range);

#endif
