/* setjmp/longjmp for x86-64 (System V ABI): rbx, rbp, r12-r15, the stack
 * pointer as it will be after setjmp returns, and the return address. The
 * register work has to be bare assembly; only the signal-mask half of
 * sigsetjmp and siglongjmp is C. */
#include <setjmp.h>
#include <stddef.h>

_Static_assert(offsetof(struct novac_jmp, __mask_saved) == 64, "setjmp.c assembly offsets");
_Static_assert(offsetof(struct novac_jmp, __mask) == 72, "setjmp.c assembly offsets");

/* Reached by a tail jump from sigsetjmp, so its return goes straight back
 * to sigsetjmp's caller, with 0. */
__attribute__((used)) static int novac__sigsetjmp_save(struct novac_jmp *env)
{
    sigprocmask(SIG_BLOCK, NULL, &env->__mask);
    return 0;
}

__asm__(
    ".text\n"
    ".globl setjmp\n .type setjmp,@function\n"
    ".globl _setjmp\n .type _setjmp,@function\n"
    "setjmp:\n"
    "_setjmp:\n"
    "    movl $0, 64(%rdi)\n"
    "novac__setjmp_regs:\n"
    "    movq %rbx, 0(%rdi)\n"
    "    movq %rbp, 8(%rdi)\n"
    "    movq %r12, 16(%rdi)\n"
    "    movq %r13, 24(%rdi)\n"
    "    movq %r14, 32(%rdi)\n"
    "    movq %r15, 40(%rdi)\n"
    "    leaq 8(%rsp), %rdx\n"           /* rsp once we have returned */
    "    movq %rdx, 48(%rdi)\n"
    "    movq (%rsp), %rdx\n"            /* where we return to */
    "    movq %rdx, 56(%rdi)\n"
    "    xorl %eax, %eax\n"
    "    ret\n"
    ".size setjmp, .-setjmp\n"

    ".globl sigsetjmp\n .type sigsetjmp,@function\n"
    "sigsetjmp:\n"
    "    movl %esi, 64(%rdi)\n"
    "    movq %rbx, 0(%rdi)\n"
    "    movq %rbp, 8(%rdi)\n"
    "    movq %r12, 16(%rdi)\n"
    "    movq %r13, 24(%rdi)\n"
    "    movq %r14, 32(%rdi)\n"
    "    movq %r15, 40(%rdi)\n"
    "    leaq 8(%rsp), %rdx\n"
    "    movq %rdx, 48(%rdi)\n"
    "    movq (%rsp), %rdx\n"
    "    movq %rdx, 56(%rdi)\n"
    "    xorl %eax, %eax\n"
    "    testl %esi, %esi\n"
    "    jz 1f\n"
    "    jmp novac__sigsetjmp_save\n"   /* rdi is still env */
    "1:  ret\n"
    ".size sigsetjmp, .-sigsetjmp\n"

    ".globl longjmp\n .type longjmp,@function\n"
    ".globl _longjmp\n .type _longjmp,@function\n"
    "longjmp:\n"
    "_longjmp:\n"
    "    movl %esi, %eax\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    incl %eax\n"                    /* longjmp(env, 0) returns 1 */
    "1:  movq 0(%rdi), %rbx\n"
    "    movq 8(%rdi), %rbp\n"
    "    movq 16(%rdi), %r12\n"
    "    movq 24(%rdi), %r13\n"
    "    movq 32(%rdi), %r14\n"
    "    movq 40(%rdi), %r15\n"
    "    movq 48(%rdi), %rsp\n"
    "    jmpq *56(%rdi)\n"
    ".size longjmp, .-longjmp\n"
);

_Noreturn void siglongjmp(sigjmp_buf env, int val)
{
    if (env->__mask_saved) sigprocmask(SIG_SETMASK, &env->__mask, NULL);
    longjmp(env, val);
}
