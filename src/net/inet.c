/* Byte order, address text conversion, and numeric getaddrinfo. No system
 * calls: this is part of the portable core and is host-tested against
 * glibc. */
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

uint16_t htons(uint16_t v) { return __builtin_bswap16(v); }
uint16_t ntohs(uint16_t v) { return __builtin_bswap16(v); }
uint32_t htonl(uint32_t v) { return __builtin_bswap32(v); }
uint32_t ntohl(uint32_t v) { return __builtin_bswap32(v); }

const struct in6_addr in6addr_any = { { { 0 } } };
const struct in6_addr in6addr_loopback = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } };

static int pton4(const char *s, unsigned char out[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        unsigned v = 0;
        int digits = 0;
        if (!isdigit((unsigned char)*s)) return 0;
        if (s[0] == '0' && isdigit((unsigned char)s[1])) return 0;   /* no leading zeros */
        while (isdigit((unsigned char)*s)) {
            v = v * 10 + (unsigned)(*s++ - '0');
            if (++digits > 3 || v > 255) return 0;
        }
        out[i] = (unsigned char)v;
        if (i < 3) { if (*s != '.') return 0; s++; }
    }
    return *s == 0;
}

static int hexval(int c)
{
    if (isdigit(c)) return c - '0';
    c = tolower(c);
    return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
}

static int pton6(const char *s, unsigned char out[16])
{
    uint16_t w[8], full[8] = { 0 };
    int n = 0, gap = -1, i;
    if (s[0] == ':') {
        if (s[1] != ':') return 0;
        s += 2; gap = 0;
        if (!*s) goto done;
    }
    for (;;) {
        const char *start = s;
        unsigned v = 0;
        int k = 0, d;
        while ((d = hexval((unsigned char)*s)) >= 0) {
            if (++k > 4) return 0;
            v = v * 16 + (unsigned)d;
            s++;
        }
        if (*s == '.') {                          /* a trailing dotted IPv4 address */
            unsigned char v4[4];
            if (n > 6 || !pton4(start, v4)) return 0;
            w[n++] = (uint16_t)(v4[0] << 8 | v4[1]);
            w[n++] = (uint16_t)(v4[2] << 8 | v4[3]);
            goto done;
        }
        if (k == 0 || n == 8) return 0;
        w[n++] = (uint16_t)v;
        if (!*s) goto done;
        if (*s != ':') return 0;
        s++;
        if (*s == ':') {
            if (gap >= 0) return 0;
            gap = n; s++;
            if (!*s) goto done;
        } else if (!*s) return 0;
    }
done:
    if (gap < 0 ? n != 8 : n > 7) return 0;
    {
        int tail = gap < 0 ? 0 : n - gap;
        for (i = 0; i < (gap < 0 ? n : gap); i++) full[i] = w[i];
        for (i = 0; i < tail; i++) full[8 - tail + i] = w[gap + i];
    }
    for (i = 0; i < 8; i++) { out[2 * i] = (unsigned char)(full[i] >> 8); out[2 * i + 1] = (unsigned char)full[i]; }
    return 1;
}

int inet_pton(int af, const char *restrict src, void *restrict dst)
{
    if (af == AF_INET) return pton4(src, dst);
    if (af == AF_INET6) return pton6(src, dst);
    errno = EAFNOSUPPORT;
    return -1;
}

const char *inet_ntop(int af, const void *restrict src, char *restrict dst, socklen_t size)
{
    char buf[INET6_ADDRSTRLEN];
    const unsigned char *a = src;
    int n;
    if (af == AF_INET) {
        n = snprintf(buf, sizeof buf, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    } else if (af == AF_INET6) {
        uint16_t w[8];
        int i, best = -1, bestlen = 0, cur = -1, curlen = 0;
        char *p = buf;
        for (i = 0; i < 8; i++) w[i] = (uint16_t)(a[2 * i] << 8 | a[2 * i + 1]);
        for (i = 0; i < 8; i++) {                 /* longest run of zero words, length >= 2 */
            if (w[i] == 0) { if (cur < 0) { cur = i; curlen = 0; } curlen++;
                             if (curlen > bestlen) { best = cur; bestlen = curlen; } }
            else cur = -1;
        }
        if (bestlen < 2) best = -1;
        if (best == 0 && (bestlen == 6 || (bestlen == 5 && w[5] == 0xffff))) {   /* IPv4-compatible/mapped */
            p += sprintf(p, "::%s%u.%u.%u.%u", bestlen == 5 ? "ffff:" : "", a[12], a[13], a[14], a[15]);
        } else {
            for (i = 0; i < 8; i++) {
                if (i == best) { *p++ = ':'; if (i == 0) *p++ = ':'; i += bestlen - 1; continue; }
                p += sprintf(p, "%x", w[i]);
                if (i < 7) *p++ = ':';
            }
            *p = 0;
        }
        n = (int)strlen(buf);
    } else {
        errno = EAFNOSUPPORT;
        return NULL;
    }
    if ((socklen_t)n >= size) { errno = ENOSPC; return NULL; }
    memcpy(dst, buf, (size_t)n + 1);
    return dst;
}

/* inet_aton: "a", "a.b", "a.b.c", "a.b.c.d", each part decimal, octal or hex. */
int inet_aton(const char *s, struct in_addr *out)
{
    unsigned long parts[4];
    int n = 0;
    for (;;) {
        char *end;
        if (!isdigit((unsigned char)*s)) return 0;
        errno = 0;
        parts[n] = strtoul(s, &end, 0);
        if (errno || end == s) return 0;
        n++;
        s = end;
        if (*s == '.') { if (n == 4) return 0; s++; continue; }
        if (*s && !isspace((unsigned char)*s)) return 0;   /* like glibc: stop at white space */
        break;
    }
    {
        unsigned long v;
        switch (n) {
        case 1: v = parts[0]; if (v > 0xffffffffUL) return 0; break;
        case 2: if (parts[0] > 255 || parts[1] > 0xffffff) return 0; v = parts[0] << 24 | parts[1]; break;
        case 3: if (parts[0] > 255 || parts[1] > 255 || parts[2] > 0xffff) return 0;
                v = parts[0] << 24 | parts[1] << 16 | parts[2]; break;
        default: if (parts[0] > 255 || parts[1] > 255 || parts[2] > 255 || parts[3] > 255) return 0;
                 v = parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3]; break;
        }
        if (out) out->s_addr = htonl((uint32_t)v);
    }
    return 1;
}

in_addr_t inet_addr(const char *s)
{
    struct in_addr a;
    return inet_aton(s, &a) ? a.s_addr : INADDR_NONE;
}

char *inet_ntoa(struct in_addr in)
{
    static char buf[INET_ADDRSTRLEN];
    return (char *)inet_ntop(AF_INET, &in, buf, sizeof buf);
}

/* ---- getaddrinfo ---------------------------------------------------------- */

struct ai_block { struct addrinfo ai; struct sockaddr_storage ss; };

static int add_result(struct addrinfo ***tail, int family, const void *addr, int port,
                      int socktype, int protocol, const char *canon)
{
    struct ai_block *b = calloc(1, sizeof *b);
    if (!b) return EAI_MEMORY;
    b->ai.ai_family = family;
    b->ai.ai_socktype = socktype;
    b->ai.ai_protocol = protocol;
    b->ai.ai_addr = (struct sockaddr *)&b->ss;
    if (family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&b->ss;
        sin->sin_family = AF_INET;
        sin->sin_port = htons((uint16_t)port);
        memcpy(&sin->sin_addr, addr, 4);
        b->ai.ai_addrlen = sizeof *sin;
    } else {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&b->ss;
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons((uint16_t)port);
        memcpy(&sin6->sin6_addr, addr, 16);
        b->ai.ai_addrlen = sizeof *sin6;
    }
    if (canon) {
        b->ai.ai_canonname = strdup(canon);
        if (!b->ai.ai_canonname) { free(b); return EAI_MEMORY; }
    }
    **tail = &b->ai;
    *tail = &b->ai.ai_next;
    return 0;
}

void freeaddrinfo(struct addrinfo *ai)
{
    while (ai) {
        struct addrinfo *next = ai->ai_next;
        free(ai->ai_canonname);
        free(ai);                         /* the ai_block starts with the addrinfo */
        ai = next;
    }
}

int getaddrinfo(const char *restrict host, const char *restrict service,
                const struct addrinfo *restrict hints, struct addrinfo **restrict res)
{
    int family = hints ? hints->ai_family : AF_UNSPEC;
    int socktype = hints ? hints->ai_socktype : 0, protocol = hints ? hints->ai_protocol : 0;
    int flags = hints ? hints->ai_flags : 0, port = 0, rc, t;
    unsigned char a4[4], a6[16];
    int have4 = 0, have6 = 0;
    struct addrinfo *head = NULL, **tail = &head;
    static const int types[][2] = { { SOCK_STREAM, IPPROTO_TCP }, { SOCK_DGRAM, IPPROTO_UDP } };

    *res = NULL;
    if (!host && !service) return EAI_NONAME;
    if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6) return EAI_FAMILY;
    if (socktype && socktype != SOCK_STREAM && socktype != SOCK_DGRAM && socktype != SOCK_RAW) return EAI_SOCKTYPE;
    if (service && *service) {
        char *end;
        long v = strtol(service, &end, 10);
        if (*end || v < 0 || v > 65535) return (flags & AI_NUMERICSERV) ? EAI_NONAME : EAI_SERVICE;
        port = (int)v;
    }
    if (!host || !*host) {
        uint32_t any = htonl((flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK);
        memcpy(a4, &any, 4); have4 = 1;
        memcpy(a6, (flags & AI_PASSIVE) ? &in6addr_any : &in6addr_loopback, 16); have6 = 1;
    } else if (pton4(host, a4)) {
        have4 = 1;
    } else if (pton6(host, a6)) {
        have6 = 1;
    } else if (!(flags & AI_NUMERICHOST) && strcasecmp(host, "localhost") == 0) {
        uint32_t lo = htonl(INADDR_LOOPBACK);
        memcpy(a4, &lo, 4); have4 = 1;
        memcpy(a6, &in6addr_loopback, 16); have6 = 1;
    } else {
        return EAI_NONAME;
    }
    if (family == AF_INET) have6 = 0;
    if (family == AF_INET6) have4 = 0;
    if (!have4 && !have6) return EAI_NONAME;      /* e.g. "1.2.3.4" with AF_INET6 */

    for (t = 0; t < 2; t++) {
        if (socktype && socktype != types[t][0]) continue;
        if (have6 && (rc = add_result(&tail, AF_INET6, a6, port, types[t][0], types[t][1],
                                      (flags & AI_CANONNAME) && !head ? host : NULL))) goto fail;
        if (have4 && (rc = add_result(&tail, AF_INET, a4, port, types[t][0], types[t][1],
                                      (flags & AI_CANONNAME) && !head ? host : NULL))) goto fail;
    }
    if (socktype == SOCK_RAW) {
        if (have4 && (rc = add_result(&tail, AF_INET, a4, port, SOCK_RAW, protocol, NULL))) goto fail;
    }
    *res = head;
    return 0;
fail:
    freeaddrinfo(head);
    return rc;
}

int getnameinfo(const struct sockaddr *restrict sa, socklen_t salen, char *restrict host,
                socklen_t hostlen, char *restrict serv, socklen_t servlen, int flags)
{
    const void *addr;
    int port;
    if (sa->sa_family == AF_INET && salen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *s = (const struct sockaddr_in *)sa;
        addr = &s->sin_addr; port = ntohs(s->sin_port);
    } else if (sa->sa_family == AF_INET6 && salen >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6 *s = (const struct sockaddr_in6 *)sa;
        addr = &s->sin6_addr; port = ntohs(s->sin6_port);
    } else return EAI_FAMILY;
    if (host && hostlen) {
        if (flags & NI_NAMEREQD) return EAI_NONAME;
        if (!inet_ntop(sa->sa_family, addr, host, hostlen)) return EAI_OVERFLOW;
    }
    if (serv && servlen) {
        if (snprintf(serv, servlen, "%d", port) >= (int)servlen) return EAI_OVERFLOW;
    }
    return 0;
}

const char *gai_strerror(int code)
{
    switch (code) {
    case 0:            return "Success";
    case EAI_BADFLAGS: return "Bad value for ai_flags";
    case EAI_NONAME:   return "Name or service not known";
    case EAI_AGAIN:    return "Temporary failure in name resolution";
    case EAI_FAIL:     return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:   return "ai_family not supported";
    case EAI_SOCKTYPE: return "ai_socktype not supported";
    case EAI_SERVICE:  return "Servname not supported for ai_socktype";
    case EAI_MEMORY:   return "Memory allocation failure";
    case EAI_SYSTEM:   return "System error";
    case EAI_OVERFLOW: return "Argument buffer overflow";
    default:           return "Unknown error";
    }
}
