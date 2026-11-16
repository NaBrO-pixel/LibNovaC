/* <signal.h>: POSIX signals on NovarisOS, which delivers them with Linux's
 * x86-64 frame layout and numbering (kernel/signal64.c). */
#ifndef NOVAC_SIGNAL_H
#define NOVAC_SIGNAL_H
#include <sys/types.h>
#include <time.h>
#include <novac/defs.h>

#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGSTKFLT 16
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31
#define SIGRTMIN  32
#define SIGRTMAX  64
#define NSIG      65

typedef int sig_atomic_t;
typedef unsigned long sigset_t;        /* the kernel's 64-signal set */

typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_NOCLDSTOP 0x00000001
#define SA_SIGINFO   0x00000004
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000

typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    int __pad0;
    union {
        struct { pid_t si_pid; uid_t si_uid; } __kill;
        void *si_addr;
        char __size[112];
    } __u;
} siginfo_t;
#define si_pid  __u.__kill.si_pid
#define si_uid  __u.__kill.si_uid
#define si_addr __u.si_addr

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t *, void *);
    } __sa;
    sigset_t sa_mask;
    int      sa_flags;
};
#define sa_handler   __sa.sa_handler
#define sa_sigaction __sa.sa_sigaction

typedef struct {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;
#define SS_ONSTACK  1
#define SS_DISABLE  2
#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192

NOVAC_BEGIN_DECLS
sighandler_t signal(int sig, sighandler_t handler);
int sigaction(int sig, const struct sigaction *restrict act,
              struct sigaction *restrict old);
int raise(int sig);
int kill(pid_t pid, int sig);
int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict old);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
int sigdelset(sigset_t *set, int sig);
int sigismember(const sigset_t *set, int sig);
int sigsuspend(const sigset_t *mask);
int sigpending(sigset_t *set);
int sigaltstack(const stack_t *restrict ss, stack_t *restrict old);
int sigwait(const sigset_t *restrict set, int *restrict sig);
int sigwaitinfo(const sigset_t *restrict set, siginfo_t *restrict info);
int sigtimedwait(const sigset_t *restrict set, siginfo_t *restrict info,
                 const struct timespec *restrict timeout);
/* Makes `sig` interrupt system calls (flag 1) or restart them (flag 0). */
int siginterrupt(int sig, int flag);
int killpg(pid_t pgrp, int sig);
/* "s: message" on stderr, like perror for signals. */
void psignal(int sig, const char *s);
NOVAC_END_DECLS
#endif
