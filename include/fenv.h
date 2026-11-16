/* <fenv.h> for x86-64: the SSE unit (MXCSR) and the x87 unit are kept in
 * step, as glibc does. libNovaC's own math functions assume
 * round-to-nearest. */
#ifndef NOVAC_FENV_H
#define NOVAC_FENV_H
#include <novac/defs.h>

#define FE_INVALID   0x01
#define FE_DIVBYZERO 0x04
#define FE_OVERFLOW  0x08
#define FE_UNDERFLOW 0x10
#define FE_INEXACT   0x20
#define FE_ALL_EXCEPT 0x3d

#define FE_TONEAREST  0x000
#define FE_DOWNWARD   0x400
#define FE_UPWARD     0x800
#define FE_TOWARDZERO 0xc00

typedef unsigned short fexcept_t;
typedef struct {
    unsigned short control, __r1, status, __r2, tags, __r3;
    unsigned int ip; unsigned short cs, opcode; unsigned int dp;
    unsigned short ds, __r4;
    unsigned int mxcsr;
} fenv_t;
#define FE_DFL_ENV ((const fenv_t *)-1)

NOVAC_BEGIN_DECLS
int feclearexcept(int excepts);
int fegetexceptflag(fexcept_t *flag, int excepts);
int feraiseexcept(int excepts);
int fesetexceptflag(const fexcept_t *flag, int excepts);
int fetestexcept(int excepts);
int fegetround(void);
int fesetround(int mode);
int fegetenv(fenv_t *env);
int feholdexcept(fenv_t *env);
int fesetenv(const fenv_t *env);
int feupdateenv(const fenv_t *env);
NOVAC_END_DECLS
#endif
