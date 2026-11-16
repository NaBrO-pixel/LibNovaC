/* BSD sockets: thin wrappers over the Linux x86-64 socket calls. */
#include <sys/socket.h>
#include <errno.h>
#include "../internal/syscall.h"

int socket(int d, int t, int p)                 { return (int)novac_sys3(NOVAC_SYS_socket, d, t, p); }
int socketpair(int d, int t, int p, int fds[2]) { return (int)novac_sys4(NOVAC_SYS_socketpair, d, t, p, fds); }
int bind(int fd, const struct sockaddr *a, socklen_t l)    { return (int)novac_sys3(NOVAC_SYS_bind, fd, a, l); }
int listen(int fd, int backlog)                 { return (int)novac_sys2(NOVAC_SYS_listen, fd, backlog); }
int connect(int fd, const struct sockaddr *a, socklen_t l) { return (int)novac_sys3(NOVAC_SYS_connect, fd, a, l); }
int accept(int fd, struct sockaddr *restrict a, socklen_t *restrict l)
{ return (int)novac_sys3(NOVAC_SYS_accept, fd, a, l); }
int accept4(int fd, struct sockaddr *restrict a, socklen_t *restrict l, int flags)
{ return (int)novac_sys4(NOVAC_SYS_accept4, fd, a, l, flags); }
ssize_t sendto(int fd, const void *b, size_t n, int f, const struct sockaddr *to, socklen_t tl)
{ return novac_sys6(NOVAC_SYS_sendto, fd, b, n, f, to, tl); }
ssize_t recvfrom(int fd, void *restrict b, size_t n, int f, struct sockaddr *restrict from, socklen_t *restrict fl)
{ return novac_sys6(NOVAC_SYS_recvfrom, fd, b, n, f, from, fl); }
ssize_t send(int fd, const void *b, size_t n, int f) { return sendto(fd, b, n, f, NULL, 0); }
ssize_t recv(int fd, void *b, size_t n, int f)       { return recvfrom(fd, b, n, f, NULL, NULL); }
ssize_t sendmsg(int fd, const struct msghdr *m, int f) { return novac_sys3(NOVAC_SYS_sendmsg, fd, m, f); }
ssize_t recvmsg(int fd, struct msghdr *m, int f)       { return novac_sys3(NOVAC_SYS_recvmsg, fd, m, f); }
int shutdown(int fd, int how) { return (int)novac_sys2(NOVAC_SYS_shutdown, fd, how); }
int getsockname(int fd, struct sockaddr *restrict a, socklen_t *restrict l)
{ return (int)novac_sys3(NOVAC_SYS_getsockname, fd, a, l); }
int getpeername(int fd, struct sockaddr *restrict a, socklen_t *restrict l)
{ return (int)novac_sys3(NOVAC_SYS_getpeername, fd, a, l); }
int setsockopt(int fd, int lv, int nm, const void *v, socklen_t l)
{ return (int)novac_sys5(NOVAC_SYS_setsockopt, fd, lv, nm, v, l); }
int getsockopt(int fd, int lv, int nm, void *restrict v, socklen_t *restrict l)
{ return (int)novac_sys5(NOVAC_SYS_getsockopt, fd, lv, nm, v, l); }
