/* <fenv.h> on x86-64: exception flags are the union of the x87 status
 * word and MXCSR; control changes go to both units. */
#include <fenv.h>

static unsigned get_mxcsr(void) { unsigned m; __asm__ volatile ("stmxcsr %0" : "=m"(m)); return m; }
static void set_mxcsr(unsigned m) { __asm__ volatile ("ldmxcsr %0" : : "m"(m)); }
static unsigned short x87_status(void) { unsigned short s; __asm__ volatile ("fnstsw %0" : "=m"(s)); return s; }

int feclearexcept(int e)
{
    fenv_t env;
    e &= FE_ALL_EXCEPT;
    __asm__ volatile ("fnstenv %0" : "=m"(env));
    env.status &= (unsigned short)~e;
    __asm__ volatile ("fldenv %0" : : "m"(env));
    set_mxcsr(get_mxcsr() & ~(unsigned)e);
    return 0;
}

int fetestexcept(int e)
{
    return (int)((x87_status() | get_mxcsr()) & (unsigned)(e & FE_ALL_EXCEPT));
}

int feraiseexcept(int e)
{
    /* Raise through real operations so traps, if enabled, fire. */
    volatile double zero = 0.0, one = 1.0, big = 0x1p1023, tiny = 0x1p-1022, r;
    if (e & FE_INVALID)   r = zero / zero;
    if (e & FE_DIVBYZERO) r = one / zero;
    if (e & FE_OVERFLOW)  r = big * big;
    if (e & FE_UNDERFLOW) r = tiny * tiny;
    if (e & FE_INEXACT)   r = one / 3.0;
    (void)r;
    return 0;
}

int fegetexceptflag(fexcept_t *f, int e) { *f = (fexcept_t)fetestexcept(e); return 0; }

int fesetexceptflag(const fexcept_t *f, int e)
{
    fenv_t env;
    e &= FE_ALL_EXCEPT;
    __asm__ volatile ("fnstenv %0" : "=m"(env));
    env.status = (unsigned short)((env.status & ~e) | (*f & e));
    __asm__ volatile ("fldenv %0" : : "m"(env));
    set_mxcsr((get_mxcsr() & ~(unsigned)e) | (unsigned)(*f & e));
    return 0;
}

int fegetround(void) { return (int)(get_mxcsr() >> 3) & 0xc00; }

int fesetround(int mode)
{
    unsigned short cw;
    if (mode & ~0xc00) return -1;
    __asm__ volatile ("fnstcw %0" : "=m"(cw));
    cw = (unsigned short)((cw & ~0xc00) | mode);
    __asm__ volatile ("fldcw %0" : : "m"(cw));
    set_mxcsr((get_mxcsr() & ~0x6000u) | ((unsigned)mode << 3));
    return 0;
}

int fegetenv(fenv_t *env)
{
    __asm__ volatile ("fnstenv %0\n\tfldenv %0" : "=m"(*env));   /* fnstenv masks; reload */
    env->mxcsr = get_mxcsr();
    return 0;
}

int fesetenv(const fenv_t *env)
{
    if (env == FE_DFL_ENV) {
        fenv_t d;
        __asm__ volatile ("fnstenv %0" : "=m"(d));
        d.control = 0x37f; d.status = 0; d.tags = 0xffff;
        __asm__ volatile ("fldenv %0" : : "m"(d));
        set_mxcsr(0x1f80);
        return 0;
    }
    __asm__ volatile ("fldenv %0" : : "m"(*env));
    set_mxcsr(env->mxcsr);
    return 0;
}

int feholdexcept(fenv_t *env)
{
    fegetenv(env);
    feclearexcept(FE_ALL_EXCEPT);
    { unsigned short cw; __asm__ volatile ("fnstcw %0" : "=m"(cw)); cw |= 0x3f; __asm__ volatile ("fldcw %0" : : "m"(cw)); }
    set_mxcsr(get_mxcsr() | 0x1f80);     /* mask every trap */
    return 0;
}

int feupdateenv(const fenv_t *env)
{
    int raised = fetestexcept(FE_ALL_EXCEPT);
    fesetenv(env);
    feraiseexcept(raised);
    return 0;
}
