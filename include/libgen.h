#ifndef NOVAC_LIBGEN_H
#define NOVAC_LIBGEN_H
#include <novac/defs.h>

NOVAC_BEGIN_DECLS
/* POSIX: may modify `path` and may return a pointer into it or to static
 * storage ("." or "/"). */
char *basename(char *path);
char *dirname(char *path);
NOVAC_END_DECLS
#endif
