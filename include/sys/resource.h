#ifndef NOVAC_SYS_RESOURCE_H
#define NOVAC_SYS_RESOURCE_H
#include <sys/types.h>
#include <sys/time.h>
#include <novac/defs.h>

typedef unsigned long rlim_t;
struct rlimit { rlim_t rlim_cur, rlim_max; };
#define RLIM_INFINITY (~0UL)
#define RLIMIT_CPU     0
#define RLIMIT_FSIZE   1
#define RLIMIT_DATA    2
#define RLIMIT_STACK   3
#define RLIMIT_CORE    4
#define RLIMIT_NOFILE  7
#define RLIMIT_AS      9

struct rusage {
    struct timeval ru_utime, ru_stime;
    long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss, ru_minflt, ru_majflt, ru_nswap,
         ru_inblock, ru_oublock, ru_msgsnd, ru_msgrcv, ru_nsignals, ru_nvcsw, ru_nivcsw;
};
#define RUSAGE_SELF      0
#define RUSAGE_CHILDREN  (-1)

NOVAC_BEGIN_DECLS
int getrlimit(int resource, struct rlimit *r);
int setrlimit(int resource, const struct rlimit *r);
int prlimit(pid_t pid, int resource, const struct rlimit *newr, struct rlimit *old);
int getrusage(int who, struct rusage *u);
NOVAC_END_DECLS
#endif
