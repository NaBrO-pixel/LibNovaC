#ifndef NOVAC_SYS_TIMES_H
#define NOVAC_SYS_TIMES_H
#include <sys/types.h>
#include <novac/defs.h>

typedef long clock_t;
struct tms { clock_t tms_utime, tms_stime, tms_cutime, tms_cstime; };

NOVAC_BEGIN_DECLS
clock_t times(struct tms *t);
NOVAC_END_DECLS
#endif
