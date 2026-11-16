#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "harness.h"

void test_scan(void)
{
    int a = 0, b = 0, n = -1;
    unsigned u = 0;
    long l = 0;
    long long ll = 0;
    short sh = 0;
    signed char hh = 0;
    double d = 0;
    float fl = 0;
    long double ld = 0;
    char s1[32], s2[32], c3[4];
    void *p = NULL;
    size_t z = 0;
    char *m = NULL;

    CHECK_EQ(sscanf("12 34", "%d %d", &a, &b), 2); CHECK_EQ(a, 12); CHECK_EQ(b, 34);
    CHECK_EQ(sscanf("  -7,+8", "%d,%d", &a, &b), 2); CHECK_EQ(a, -7); CHECK_EQ(b, 8);
    CHECK_EQ(sscanf("0x1f 017 9", "%i %i %i", &a, &b, &n), 3); CHECK_EQ(a, 31); CHECK_EQ(b, 15); CHECK_EQ(n, 9);
    CHECK_EQ(sscanf("ff", "%x", &u), 1); CHECK_EQ(u, 255);
    CHECK_EQ(sscanf("0XFF", "%X", &u), 1); CHECK_EQ(u, 255);
    CHECK_EQ(sscanf("777", "%o", &u), 1); CHECK_EQ(u, 511);
    CHECK_EQ(sscanf("12345", "%3d%d", &a, &b), 2); CHECK_EQ(a, 123); CHECK_EQ(b, 45);
    CHECK_EQ(sscanf("-9223372036854775808", "%lld", &ll), 1); CHECK(ll == (-9223372036854775807LL - 1));
    CHECK_EQ(sscanf("70000 300", "%hd %hhd", &sh, &hh), 2); CHECK_EQ(sh, (short)70000); CHECK_EQ(hh, (signed char)300);
    CHECK_EQ(sscanf("123456789012", "%ld", &l), 1); CHECK_EQ(l, 123456789012L);
    CHECK_EQ(sscanf("42", "%zu", &z), 1); CHECK_EQ(z, 42);
    CHECK_EQ(sscanf("abc", "%d", &a), 0);
    CHECK_EQ(sscanf("", "%d", &a), -1);
    CHECK_EQ(sscanf("   ", "%d", &a), -1);
    CHECK_EQ(sscanf("5", "%d %d", &a, &b), 1);
    CHECK_EQ(sscanf("x", "y%d", &a), 0);
    CHECK_EQ(sscanf("0xg", "%x%s", &u, s1), 2); CHECK_EQ(u, 0); CHECK_STR(s1, "g");

    CHECK_EQ(sscanf("hello world", "%s %s", s1, s2), 2); CHECK_STR(s1, "hello"); CHECK_STR(s2, "world");
    CHECK_EQ(sscanf("abcdef", "%3s%s", s1, s2), 2); CHECK_STR(s1, "abc"); CHECK_STR(s2, "def");
    memset(c3, 'z', sizeof c3);
    CHECK_EQ(sscanf(" xyz", "%3c", c3), 1); CHECK(memcmp(c3, " xy", 3) == 0 && c3[3] == 'z');
    CHECK_EQ(sscanf("key=value;rest", "%[^=]=%[^;]", s1, s2), 2); CHECK_STR(s1, "key"); CHECK_STR(s2, "value");
    CHECK_EQ(sscanf("]]ab", "%[]]%s", s1, s2), 2); CHECK_STR(s1, "]]"); CHECK_STR(s2, "ab");
    CHECK_EQ(sscanf("a-b-c", "%[a-c-]", s1), 1); CHECK_STR(s1, "a-b-c");
    CHECK_EQ(sscanf("123abc", "%*d%s", s1), 1); CHECK_STR(s1, "abc");
    CHECK_EQ(sscanf("12 34", "%d%n %d%n", &a, &n, &b, &u), 2); CHECK_EQ(n, 2); CHECK_EQ(u, 5);
    CHECK_EQ(sscanf("100%", "%d%%", &a), 1);
    CHECK_EQ(sscanf("a  b", "a b%n", &n), 0); CHECK_EQ(n, 4);

    CHECK_EQ(sscanf("3.25 -1e3 0x1.8p1", "%lf %f %La", &d, &fl, &ld), 3);
    CHECK(d == 3.25 && fl == -1000.0f && ld == 3.0L);
    CHECK_EQ(sscanf("inf nan -Infinity", "%lf %f %lf", &d, &fl, &d), 3); CHECK(d < -1e308 && fl != fl);
    CHECK_EQ(sscanf("100ergs", "%lf%s", &d, s1), 0);           /* C11 7.21.6.2 example */
    CHECK_EQ(sscanf("1.5e+x", "%lf", &d), 0);
    CHECK_EQ(sscanf("2.5xyz", "%lf%s", &d, s1), 2); CHECK(d == 2.5); CHECK_STR(s1, "xyz");
    CHECK_EQ(sscanf("1.23456", "%4lf", &d), 1); CHECK(d == 1.23);
    CHECK_EQ(sscanf("0.1", "%lf", &d), 1); CHECK(d == 0.1);

    { char buf[32]; void *addr = s1; snprintf(buf, sizeof buf, "%p", addr);
      CHECK_EQ(sscanf(buf, "%p", &p), 1); CHECK(p == addr); }

    CHECK_EQ(sscanf("dynamic string", "%ms %m[a-z]", &m, (char **)&p), 2);
    CHECK_STR(m, "dynamic"); CHECK_STR((char *)p, "string");
    free(m); free(p);
}
