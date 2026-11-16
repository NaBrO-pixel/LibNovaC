#ifndef NOVAC_SYS_IOCTL_H
#define NOVAC_SYS_IOCTL_H
#include <novac/defs.h>

#define TCGETS     0x5401
#define TCSETS     0x5402
#define TCSETSW    0x5403
#define TCSETSF    0x5404
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD   0x541B
#define FIONBIO    0x5421

struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };

NOVAC_BEGIN_DECLS
/* ioctl(fd, request) or ioctl(fd, request, pointer-or-int). */
int ioctl(int fd, unsigned long request, ...);
NOVAC_END_DECLS
#endif
