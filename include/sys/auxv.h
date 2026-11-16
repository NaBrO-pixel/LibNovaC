#ifndef NOVAC_SYS_AUXV_H
#define NOVAC_SYS_AUXV_H
#include <novac/defs.h>

#define AT_NULL     0
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_ENTRY    9
#define AT_UID      11
#define AT_EUID     12
#define AT_GID      13
#define AT_EGID     14
#define AT_PLATFORM 15
#define AT_HWCAP    16
#define AT_CLKTCK   17
#define AT_SECURE   23
#define AT_RANDOM   25
#define AT_HWCAP2   26
#define AT_EXECFN   31
#define AT_SYSINFO_EHDR 33

NOVAC_BEGIN_DECLS
/* The value the kernel passed for `type`, or 0 with errno ENOENT. */
unsigned long getauxval(unsigned long type);
NOVAC_END_DECLS
#endif
