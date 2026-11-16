/* <netdb.h>: getaddrinfo and getnameinfo for NUMERIC addresses (and the
 * names "localhost" and ""), plus numeric service ports. NovarisOS has no
 * resolver, so other names fail with EAI_NONAME. */
#ifndef NOVAC_NETDB_H
#define NOVAC_NETDB_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <novac/defs.h>

struct addrinfo {
    int ai_flags, ai_family, ai_socktype, ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
#define AI_PASSIVE     0x01
#define AI_CANONNAME   0x02
#define AI_NUMERICHOST 0x04
#define AI_V4MAPPED    0x08
#define AI_ALL         0x10
#define AI_ADDRCONFIG  0x20
#define AI_NUMERICSERV 0x400

#define NI_NUMERICHOST 1
#define NI_NUMERICSERV 2
#define NI_NOFQDN      4
#define NI_NAMEREQD    8
#define NI_DGRAM       16
#define NI_MAXHOST     1025
#define NI_MAXSERV     32

#define EAI_BADFLAGS   (-1)
#define EAI_NONAME     (-2)
#define EAI_AGAIN      (-3)
#define EAI_FAIL       (-4)
#define EAI_FAMILY     (-6)
#define EAI_SOCKTYPE   (-7)
#define EAI_SERVICE    (-8)
#define EAI_MEMORY     (-10)
#define EAI_SYSTEM     (-11)
#define EAI_OVERFLOW   (-12)

NOVAC_BEGIN_DECLS
int  getaddrinfo(const char *restrict host, const char *restrict service,
                 const struct addrinfo *restrict hints, struct addrinfo **restrict res);
void freeaddrinfo(struct addrinfo *ai);
int  getnameinfo(const struct sockaddr *restrict sa, socklen_t salen, char *restrict host,
                 socklen_t hostlen, char *restrict serv, socklen_t servlen, int flags);
const char *gai_strerror(int code);
NOVAC_END_DECLS
#endif
