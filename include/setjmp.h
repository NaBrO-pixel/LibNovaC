/* <setjmp.h> for x86-64. jmp_buf holds the callee-saved registers, the
 * stack pointer and the return address; sigjmp_buf adds the signal mask.
 * setjmp does not save the signal mask (Linux/glibc behaviour); use
 * sigsetjmp(env, 1) for that. */
#ifndef NOVAC_SETJMP_H
#define NOVAC_SETJMP_H
#include <signal.h>
#include <novac/defs.h>

typedef struct novac_jmp {
    unsigned long __regs[8];         /* rbx rbp r12 r13 r14 r15 rsp rip */
    int __mask_saved;
    sigset_t __mask;
} jmp_buf[1];
typedef jmp_buf sigjmp_buf;

#if defined(__GNUC__) || defined(__clang__)
# define NOVAC_RETURNS_TWICE __attribute__((returns_twice))
#else
# define NOVAC_RETURNS_TWICE
#endif

NOVAC_BEGIN_DECLS
int setjmp(jmp_buf env) NOVAC_RETURNS_TWICE;
int _setjmp(jmp_buf env) NOVAC_RETURNS_TWICE;
int sigsetjmp(sigjmp_buf env, int savemask) NOVAC_RETURNS_TWICE;
_Noreturn void longjmp(jmp_buf env, int val);
_Noreturn void _longjmp(jmp_buf env, int val);
_Noreturn void siglongjmp(sigjmp_buf env, int val);
NOVAC_END_DECLS
#endif
