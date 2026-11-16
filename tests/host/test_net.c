/* inet_pton/ntop/aton, getaddrinfo and getopt, checked against glibc where
 * glibc is a reference. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include "harness.h"
#include "oracle.h"

static uint64_t rng = 0x1234567887654321ULL;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void our_trace(int argc, char **argv, const char *opts, const struct option *lo, char *out, size_t n)
{
    int c, li, len = 0;
    optind = 0;
    opterr = 0;
    while ((c = getopt_long(argc, argv, opts, lo, &li)) != -1) {
        len += snprintf(out + len, n - (size_t)len, "%d[%s]", c, optarg ? optarg : "-");
        if (len > (int)n - 64) break;
    }
    snprintf(out + len, n - (size_t)len, "|%d", optind);
}

static void test_getopt(void)
{
    static int flag;
    static const struct option lo[] = {
        { "verbose", no_argument, &flag, 7 }, { "file", required_argument, NULL, 'f' },
        { "level", optional_argument, NULL, 'L' }, { "verify", no_argument, NULL, 'V' },
        { NULL, 0, NULL, 0 } };
    static const char *const words[] = { "-a", "-b", "-c", "-ab", "-cfoo", "-f", "x", "y", "--",
        "--verbose", "--file", "--file=z", "--level", "--level=3", "--ver", "--verb", "-", "-q", "--nope", "-bc" };
    static const char *const optss[] = { "abc:f:", "+abc:f:", "-abc:f:", ":abc:f:", "ab::c:", "abcf:" };
    int k, bad = 0;
    for (k = 0; k < 20000; k++) {
        char *a1[12], *a2[12], t1[1024], t2[1024];
        int argc = 1 + (int)(next() % 9), i;
        const char *opts = optss[next() % 6];
        const struct option *l = (next() & 1) ? lo : NULL;
        a1[0] = a2[0] = (char *)"prog";
        for (i = 1; i < argc; i++) a1[i] = a2[i] = (char *)words[next() % 20];
        a1[argc] = a2[argc] = NULL;
        our_trace(argc, a1, opts, l, t1, sizeof t1);
        oracle_getopt_trace(argc, a2, opts, l, t2, sizeof t2);
        for (i = 0; i < argc; i++) if (strcmp(a1[i], a2[i]) != 0) { strcat(t1, " argv"); break; }
        if (strcmp(t1, t2) != 0) {
            if (bad++ < 6) {
                th_note("  getopt(\"%s\", long=%d):", opts, l != NULL);
                for (i = 1; i < argc; i++) th_note(" %s", a2[i]);
                th_note("\n    ours  %s\n    glibc %s\n", t1, t2);
            }
        }
    }
    CHECK_EQ(bad, 0);
}

void test_net(void)
{
    unsigned char a[16], b[16];
    char s[64], t[64];
    int k, bad = 0;
    struct addrinfo hints, *res, *r;

    CHECK_EQ(htons(0x1234), 0x3412);
    CHECK_EQ(ntohl(0x01020304u), 0x04030201u);
    CHECK_EQ(inet_pton(AF_INET, "192.168.1.20", a), 1);
    CHECK(a[0] == 192 && a[3] == 20);
    CHECK_EQ(inet_pton(AF_INET, "256.1.1.1", a), 0);
    CHECK_EQ(inet_pton(AF_INET, "1.2.3", a), 0);
    CHECK_EQ(inet_pton(AF_INET, "01.2.3.4", a), 0);
    CHECK_EQ(inet_pton(99, "1.2.3.4", a), -1);
    CHECK_EQ(inet_pton(AF_INET6, "::1", a), 1); CHECK(a[15] == 1 && a[0] == 0);
    CHECK_STR(inet_ntop(AF_INET6, a, s, sizeof s), "::1");
    CHECK_EQ(inet_pton(AF_INET6, "2001:db8::ff00:42:8329", a), 1);
    CHECK_STR(inet_ntop(AF_INET6, a, s, sizeof s), "2001:db8::ff00:42:8329");
    CHECK_EQ(inet_pton(AF_INET6, "::ffff:10.0.0.1", a), 1);
    CHECK_STR(inet_ntop(AF_INET6, a, s, sizeof s), "::ffff:10.0.0.1");
    CHECK(inet_ntop(AF_INET, "\x01\x02\x03\x04", s, 7) == NULL);
    CHECK_EQ(inet_addr("10.1"), htonl(0x0a000001));
    CHECK_EQ(inet_addr("0x7f.1"), htonl(0x7f000001));
    CHECK_EQ(inet_addr("1.2.3.256"), INADDR_NONE);
    { struct in_addr in; in.s_addr = htonl(0xc0a80001); CHECK_STR(inet_ntoa(in), "192.168.0.1"); }

    /* random text, and random addresses, against glibc */
    for (k = 0; k < 30000; k++) {
        static const char alpha[] = "0123456789abcdefABCDEF:.x ";
        int len = 1 + (int)(next() % 40), i, af = (next() & 1) ? AF_INET : AF_INET6, r1, r2;
        unsigned u1, u2;
        for (i = 0; i < len; i++) s[i] = alpha[next() % (sizeof alpha - 1)];
        s[len] = 0;
        if (k % 3 == 0) {          /* well-formed-ish */
            if (af == AF_INET) snprintf(s, sizeof s, "%d.%d.%d.%d", (int)(next() % 300), (int)(next() % 260), (int)(next() % 256), (int)(next() % 256));
            else {
                for (i = 0; i < 16; i++) b[i] = (next() % 3) ? 0 : (unsigned char)next();
                oracle_inet_ntop(AF_INET6, b, s, sizeof s);
            }
        }
        memset(a, 0xAA, 16); memset(b, 0xAA, 16);
        r1 = inet_pton(af, s, a); r2 = oracle_inet_pton(af, s, b);
        if (r1 != r2 || (r1 == 1 && memcmp(a, b, af == AF_INET ? 4 : 16) != 0)) {
            if (bad++ < 6) th_note("  inet_pton(%d, \"%s\"): ours %d glibc %d\n", af, s, r1, r2);
        }
        if (r1 == 1) {
            const char *x = inet_ntop(af, a, s, sizeof s), *y = oracle_inet_ntop(af, a, t, sizeof t);
            if (!x || !y || strcmp(x, y) != 0) { if (bad++ < 6) th_note("  inet_ntop: ours %s glibc %s\n", x ? x : "-", y ? y : "-"); }
        }
        if (af == AF_INET) {
            struct in_addr ia;
            r1 = inet_aton(s, &ia); u1 = ia.s_addr;
            r2 = oracle_inet_aton(s, &u2);
            if (r1 != r2 || (r1 && u1 != u2)) { if (bad++ < 6) th_note("  inet_aton(\"%s\"): ours %d glibc %d\n", s, r1, r2); }
        }
    }
    CHECK_EQ(bad, 0);

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    CHECK_EQ(getaddrinfo("127.0.0.1", "8080", &hints, &res), 0);
    CHECK(res && res->ai_family == AF_INET && res->ai_next == NULL &&
          ((struct sockaddr_in *)res->ai_addr)->sin_port == htons(8080));
    CHECK_EQ(getnameinfo(res->ai_addr, res->ai_addrlen, s, sizeof s, t, sizeof t, NI_NUMERICHOST | NI_NUMERICSERV), 0);
    CHECK_STR(s, "127.0.0.1"); CHECK_STR(t, "8080");
    freeaddrinfo(res);
    hints.ai_flags = AI_PASSIVE; hints.ai_family = AF_INET;
    CHECK_EQ(getaddrinfo(NULL, "0", &hints, &res), 0);
    CHECK(((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr == 0);
    freeaddrinfo(res);
    memset(&hints, 0, sizeof hints);
    CHECK_EQ(getaddrinfo("localhost", NULL, &hints, &res), 0);
    for (k = 0, r = res; r; r = r->ai_next) k++;
    CHECK_EQ(k, 4);                                   /* v6 and v4, stream and dgram */
    freeaddrinfo(res);
    CHECK_EQ(getaddrinfo("example.com", "80", NULL, &res), EAI_NONAME);
    CHECK_EQ(getaddrinfo("1.2.3.4", "http", NULL, &res), EAI_SERVICE);
    CHECK_STR(gai_strerror(EAI_NONAME), "Name or service not known");

    test_getopt();
}
