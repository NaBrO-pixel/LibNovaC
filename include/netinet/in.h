#ifndef NOVAC_NETINET_IN_H
#define NOVAC_NETINET_IN_H
#include <stdint.h>
#include <sys/socket.h>
#include <novac/defs.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
struct in_addr { in_addr_t s_addr; };
struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;           /* network byte order */
    struct in_addr sin_addr;      /* network byte order */
    unsigned char sin_zero[8];
};
struct in6_addr { union { uint8_t s6_addr[16]; uint16_t __s6_addr16[8]; uint32_t __s6_addr32[4]; } __in6_u; };
#define s6_addr __in6_u.s6_addr
struct sockaddr_in6 {
    sa_family_t sin6_family; in_port_t sin6_port; uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr; uint32_t sin6_scope_id;
};
extern const struct in6_addr in6addr_any, in6addr_loopback;

#define IPPROTO_IP   0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define IPPROTO_IPV6 41
#define IPPROTO_RAW  255

#define INADDR_ANY       ((in_addr_t)0x00000000)
#define INADDR_BROADCAST ((in_addr_t)0xffffffff)
#define INADDR_NONE      ((in_addr_t)0xffffffff)
#define INADDR_LOOPBACK  ((in_addr_t)0x7f000001)
#define INET_ADDRSTRLEN  16
#define INET6_ADDRSTRLEN 46

#define IP_TOS 1
#define IP_TTL 2

NOVAC_BEGIN_DECLS
uint32_t htonl(uint32_t v);
uint16_t htons(uint16_t v);
uint32_t ntohl(uint32_t v);
uint16_t ntohs(uint16_t v);
NOVAC_END_DECLS
#endif
