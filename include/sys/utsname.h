#ifndef NOVAC_SYS_UTSNAME_H
#define NOVAC_SYS_UTSNAME_H
#include <novac/defs.h>

struct utsname {
    char sysname[65], nodename[65], release[65], version[65], machine[65], domainname[65];
};

NOVAC_BEGIN_DECLS
int uname(struct utsname *u);
NOVAC_END_DECLS
#endif
