#ifndef NOVAC_STRINGS_H
#define NOVAC_STRINGS_H
#include <stddef.h>
#include <novac/defs.h>

NOVAC_BEGIN_DECLS
int  strcasecmp(const char *a, const char *b);
int  strncasecmp(const char *a, const char *b, size_t n);
void bzero(void *p, size_t n);
int  bcmp(const void *a, const void *b, size_t n);
void bcopy(const void *src, void *dst, size_t n);
int  ffs(int v);
int  ffsl(long v);
int  ffsll(long long v);
NOVAC_END_DECLS
#endif
