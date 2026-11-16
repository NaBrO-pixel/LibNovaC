#ifndef NOVAC_ARPA_INET_H
#define NOVAC_ARPA_INET_H
#include <netinet/in.h>
#include <novac/defs.h>

NOVAC_BEGIN_DECLS
/* Strict dotted-quad (AF_INET) or RFC 4291 text (AF_INET6).
 * 1 = converted, 0 = not a valid address, -1 = bad family (EAFNOSUPPORT). */
int         inet_pton(int af, const char *restrict src, void *restrict dst);
const char *inet_ntop(int af, const void *restrict src, char *restrict dst, socklen_t size);
/* The historical forms, which also accept "10.1", hex and octal parts. */
int         inet_aton(const char *s, struct in_addr *out);
in_addr_t   inet_addr(const char *s);
char       *inet_ntoa(struct in_addr in);
NOVAC_END_DECLS
#endif
