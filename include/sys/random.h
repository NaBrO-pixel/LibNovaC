#ifndef NOVAC_SYS_RANDOM_H
#define NOVAC_SYS_RANDOM_H
#include <sys/types.h>
#include <novac/defs.h>

#define GRND_NONBLOCK 1
#define GRND_RANDOM   2

NOVAC_BEGIN_DECLS
ssize_t getrandom(void *buf, size_t n, unsigned flags);
/* Fills buf (n <= 256) or fails with EIO; never returns short. */
int getentropy(void *buf, size_t n);
NOVAC_END_DECLS
#endif
