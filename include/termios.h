/* <termios.h>: the Linux x86-64 kernel termios over TCGETS/TCSETS*. */
#ifndef NOVAC_TERMIOS_H
#define NOVAC_TERMIOS_H
#include <sys/types.h>
#include <novac/defs.h>

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;
#define NCCS 19
struct termios {
    tcflag_t c_iflag, c_oflag, c_cflag, c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
};

#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSTART 8
#define VSTOP 9
#define VSUSP 10

#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK  0000020
#define ISTRIP 0000040
#define INLCR  0000100
#define IGNCR  0000200
#define ICRNL  0000400
#define IXON   0002000
#define IXANY  0004000
#define IXOFF  0010000
#define OPOST  0000001
#define ONLCR  0000004
#define CSIZE  0000060
#define CS5    0000000
#define CS6    0000020
#define CS7    0000040
#define CS8    0000060
#define CSTOPB 0000100
#define CREAD  0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL  0002000
#define CLOCAL 0004000
#define CBAUD  0010017
#define ISIG   0000001
#define ICANON 0000002
#define ECHO   0000010
#define ECHOE  0000020
#define ECHOK  0000040
#define ECHONL 0000100
#define NOFLSH 0000200
#define TOSTOP 0000400
#define IEXTEN 0100000

#define B0     0000000
#define B9600  0000015
#define B19200 0000016
#define B38400 0000017
#define B57600 0010001
#define B115200 0010002

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

NOVAC_BEGIN_DECLS
int  tcgetattr(int fd, struct termios *t);
int  tcsetattr(int fd, int when, const struct termios *t);
void cfmakeraw(struct termios *t);
speed_t cfgetispeed(const struct termios *t);
speed_t cfgetospeed(const struct termios *t);
int  cfsetispeed(struct termios *t, speed_t s);
int  cfsetospeed(struct termios *t, speed_t s);
int  cfsetspeed(struct termios *t, speed_t s);
NOVAC_END_DECLS
#endif
