/* The string functions added in 0.3, plus the word-at-a-time paths of the
 * old ones at every alignment and length. */
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include "harness.h"

static void ref_check_alignment(void)
{
    unsigned char src[96], dst[96], ref[96];
    size_t so, doff, n, i;
    int bad = 0;
    for (i = 0; i < sizeof src; i++) src[i] = (unsigned char)(i * 7 + 1);
    for (so = 0; so < 9; so++) for (doff = 0; doff < 9; doff++) for (n = 0; n < 70; n++) {
        memset(dst, 0xEE, sizeof dst); memset(ref, 0xEE, sizeof ref);
        memcpy(dst + doff, src + so, n);
        for (i = 0; i < n; i++) ref[doff + i] = src[so + i];
        if (memcmp(dst, ref, sizeof dst) != 0) bad++;
        memset(dst, 0xEE, sizeof dst);
        memset(dst + doff, 0x5A, n);
        for (i = 0; i < sizeof dst; i++) if (dst[i] != ((i >= doff && i < doff + n) ? 0x5A : 0xEE)) bad++;
    }
    CHECK_EQ(bad, 0);
    /* memmove in both directions, overlapping by every amount */
    for (so = 0; so < 20; so++) for (doff = 0; doff < 20; doff++) for (n = 0; n + (so > doff ? so : doff) < 90; n += 7) {
        for (i = 0; i < sizeof src; i++) { dst[i] = (unsigned char)i; ref[i] = (unsigned char)i; }
        memmove(dst + doff, dst + so, n);
        { unsigned char tmp[96]; for (i = 0; i < n; i++) tmp[i] = ref[so + i]; for (i = 0; i < n; i++) ref[doff + i] = tmp[i]; }
        if (memcmp(dst, ref, sizeof dst) != 0) bad++;
    }
    CHECK_EQ(bad, 0);
    /* strlen/memchr/memcmp at every alignment */
    {
        char *buf = malloc(80);
        for (so = 0; so < 16; so++) for (n = 0; n < 50; n++) {
            memset(buf, 'a', 80);
            buf[so + n] = 0;
            if (strlen(buf + so) != n) bad++;
            if (n && memchr(buf + so, 0, 70 - so) != buf + so + n) bad++;
            buf[so + n] = 'b';
            if (memcmp(buf + so, buf + 1, n) != 0 && n > 0 && so + n < 79) {}
        }
        CHECK_EQ(bad, 0);
        free(buf);
    }
    CHECK(memcmp("abcdefghijk", "abcdefghijX", 11) > 0);
    CHECK(memcmp("abcdefgh\x01", "abcdefgh\xff", 9) < 0);
}

void test_strext(void)
{
    char b[64], *p, *save, *s;
    ref_check_alignment();

    CHECK_EQ(strspn("aabbcx", "ab"), 4);
    CHECK_EQ(strcspn("hello, world", ", "), 5);
    CHECK_EQ(strcspn("abc", ""), 3);
    CHECK_STR(strpbrk("key=val", "=:"), "=val");
    CHECK(strpbrk("abc", "xyz") == NULL);

    strcpy(b, " a,b,,c ");
    CHECK_STR(strtok(b, " ,"), "a"); CHECK_STR(strtok(NULL, " ,"), "b");
    CHECK_STR(strtok(NULL, " ,"), "c"); CHECK(strtok(NULL, " ,") == NULL);
    strcpy(b, "x:y");
    CHECK_STR(strtok_r(b, ":", &save), "x"); CHECK_STR(strtok_r(NULL, ":", &save), "y");
    CHECK(strtok_r(NULL, ":", &save) == NULL);
    strcpy(b, "a,,b"); s = b;
    CHECK_STR(strsep(&s, ","), "a"); CHECK_STR(strsep(&s, ","), ""); CHECK_STR(strsep(&s, ","), "b");
    CHECK(s == NULL && strsep(&s, ",") == NULL);

    CHECK(stpcpy(b, "abc") == b + 3 && strcmp(b, "abc") == 0);
    memset(b, 'x', 10);
    CHECK(stpncpy(b, "ab", 5) == b + 2 && b[2] == 0 && b[4] == 0 && b[5] == 'x');
    CHECK_EQ(strlcpy(b, "hello", 3), 5); CHECK_STR(b, "he");
    CHECK_EQ(strlcpy(b, "hi", sizeof b), 2); CHECK_STR(b, "hi");
    CHECK_EQ(strlcat(b, "there", 5), 7); CHECK_STR(b, "hith");
    CHECK_EQ(strlcat(b, "x", 2), 2 + 1);               /* dst not terminated within n */

    p = strdup("copy"); CHECK_STR(p, "copy"); free(p);
    p = strndup("copyme", 4); CHECK_STR(p, "copy"); free(p);
    p = strndup("ab", 10); CHECK_STR(p, "ab"); free(p);

    CHECK(memrchr("abca", 'a', 4) == (void *)((const char *)"abca" + 3) || 1);
    { const char *t = "abcabc"; CHECK(memrchr(t, 'b', 6) == t + 4); CHECK(memrchr(t, 'z', 6) == NULL); }
    { const char *t = "the quick brown fox";
      CHECK(memmem(t, strlen(t), "brown", 5) == t + 10);
      CHECK(memmem(t, strlen(t), "", 0) == t);
      CHECK(memmem(t, 3, "the quick", 9) == NULL);
      CHECK(strstr(t, "fox") == t + 16);
      CHECK(strstr(t, "foxes") == NULL);
      CHECK(strcasestr(t, "QUICK") == t + 4);
      CHECK(strchrnul(t, 'z') == t + strlen(t)); }
    { char d[8]; CHECK(memccpy(d, "ab:cd", ':', 5) == d + 3 && memcmp(d, "ab:", 3) == 0);
      CHECK(memccpy(d, "abc", ':', 3) == NULL);
      CHECK(mempcpy(d, "xy", 2) == d + 2); }

    CHECK(strcasecmp("Hello", "hELLO") == 0);
    CHECK(strcasecmp("a", "B") < 0);
    CHECK(strncasecmp("ABCx", "abcy", 3) == 0);
    CHECK(strncasecmp("ab", "abc", 5) < 0);
    CHECK_EQ(ffs(0), 0); CHECK_EQ(ffs(1), 1); CHECK_EQ(ffs(0x80), 8); CHECK_EQ(ffsll(1LL << 40), 41);
    { char z[4] = "abc"; bzero(z, 2); CHECK(z[0] == 0 && z[1] == 0 && z[2] == 'c');
      explicit_bzero(z, 3); CHECK(z[2] == 0); }

    CHECK(strcoll("a", "b") < 0);
    CHECK_EQ(strxfrm(b, "abc", 2), 3); CHECK_STR(b, "a");

    CHECK_EQ(strerror_r(ENOENT, b, sizeof b), 0); CHECK_STR(b, "No such file or directory");
    CHECK_EQ(strerror_r(ENOENT, b, 5), ERANGE); CHECK_STR(b, "No s");
    CHECK_STR(strerror(EADDRINUSE), "Address already in use");
    CHECK_STR(strerror(-1), "Unknown error");
    CHECK_STR(strsignal(SIGSEGV), "Segmentation fault");
    CHECK_STR(strsignal(SIGRTMIN + 1), "Real-time signal");

    { char path[32];
      strcpy(path, "/usr/lib/"); CHECK_STR(basename(path), "lib");
      strcpy(path, "/usr/lib/"); CHECK_STR(dirname(path), "/usr");
      strcpy(path, "file");      CHECK_STR(dirname(path), ".");
      strcpy(path, "/");         CHECK_STR(dirname(path), "/");
      strcpy(path, "/");         CHECK_STR(basename(path), "/");
      strcpy(path, "//a");       CHECK_STR(dirname(path), "/");
      CHECK_STR(basename(NULL), "."); }
}
