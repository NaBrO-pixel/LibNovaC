#ifndef NOVAC_FNMATCH_H
#define NOVAC_FNMATCH_H
#include <novac/defs.h>

#define FNM_NOMATCH  1
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD   0x04
#define FNM_LEADING_DIR 0x08
#define FNM_CASEFOLD 0x10

NOVAC_BEGIN_DECLS
/* 0 if `string` matches the shell pattern, FNM_NOMATCH if not. Supports
 * * ? [...] [!...] [^...] ranges, [:class:] and backslash escapes. */
int fnmatch(const char *pattern, const char *string, int flags);
NOVAC_END_DECLS
#endif
