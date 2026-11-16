/* The target test, continued: what libNovaC 0.3 added. Same rules as
 * novac_target_test.c - libNovaC only, and output that cannot differ
 * between Linux and NovarisOS (no addresses, pids, times, host names). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <poll.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <sys/random.h>
#include <sys/auxv.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "target.h"

#define D3 "/tmp/novac_t3"

/* ---- setjmp ---------------------------------------------------------------- */

static jmp_buf jb;
static sigjmp_buf sjb;
static volatile int depth;

static void dive(int n) { depth = n; if (n == 5) longjmp(jb, 42); if (n < 100) dive(n + 1); }
static void on_usr1_jump(int sig) { (void)sig; siglongjmp(sjb, 7); }

static void test_setjmp(void)
{
    volatile int kept = 1;
    int r = setjmp(jb);
    if (r == 0) { kept = 99; dive(0); }
    ok("longjmp unwinds five frames and returns its value", r == 42 && depth == 5);
    ok("a volatile local survives the jump", kept == 99);
    r = setjmp(jb);
    if (r == 0) longjmp(jb, 0);
    ok("longjmp(env, 0) makes setjmp return 1", r == 1);
    {
        sigset_t s, now;
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = on_usr1_jump;
        sigaction(SIGUSR1, &sa, NULL);
        r = sigsetjmp(sjb, 1);
        if (r == 0) raise(SIGUSR1);                /* the handler jumps out */
        sigprocmask(SIG_BLOCK, NULL, &now);
        ok("siglongjmp out of a handler restores the mask", r == 7 && !sigismember(&now, SIGUSR1));
        r = sigsetjmp(sjb, 1);
        if (r == 0) {
            sigemptyset(&s); sigaddset(&s, SIGUSR2);
            sigprocmask(SIG_BLOCK, &s, NULL);
            siglongjmp(sjb, 1);
        }
        sigprocmask(SIG_BLOCK, NULL, &now);
        ok("sigsetjmp(env, 1) saved the mask siglongjmp restores", !sigismember(&now, SIGUSR2));
        signal(SIGUSR1, SIG_DFL);
    }
}

/* ---- floating point ---------------------------------------------------- */

static void test_float(void)
{
    char b[128];
    double d;
    float f;
    int n;
    printf("float: %.3f %e %g %G %a %.0f %.2f\n", 3.14159265, 6.02214076e23, 1e-5, 2.5e20, 1.0 / 3, 2.5, 1.005);
    printf("float: %10.4f|%-10.2e|%+g|% .3g|%#.0f|%08.3f\n", -2.71828, 299792458.0, 0.0001, 1234567.0, 3.0, -1.5);
    printf("float: %f %F %e\n", 1.0 / 0.0, -1.0 / 0.0, nan(""));
    snprintf(b, sizeof b, "%.17g", 0.1);
    ok("%.17g prints the shortest round-trip form of 0.1", strcmp(b, "0.10000000000000001") == 0);
    ok("strtod rounds 0.1 correctly", strtod("0.1", NULL) == 0.1);
    ok("strtod reads hex floats", strtod("0x1.8p3", NULL) == 12.0);
    ok("strtod: 1e400 overflows with ERANGE", (errno = 0, strtod("1e400", NULL) == HUGE_VAL && errno == ERANGE));
    ok("strtof is correctly rounded", strtof("1.0000000596046448", NULL) == 1.0f + 0x1p-23f &&
       strtof("1.0000000596046447", NULL) == 1.0f);
    n = sscanf("x=1.5e3 y=-0.25 name:novaris", "x=%lf y=%f name:%31s", &d, &f, b);
    ok("sscanf reads doubles, floats and strings", n == 3 && d == 1500.0 && f == -0.25f && strcmp(b, "novaris") == 0);
    printf("math: %.17g %.17g %.17g %.17g\n", sin(1.0), exp(1.0), log(10.0), pow(2.0, 0.5));
    printf("math: %.17g %.17g %.17g %.17g\n", atan2(1.0, 1.0) * 4, cbrt(27.0), hypot(3.0, 4.0), tanh(0.5));
    ok("sin(pi) is within 2^-52 of 0", fabs(sin(M_PI)) < 0x1p-52);
    ok("cos(1e22) uses full-precision reduction", cos(1e22) == 0x1.11edeec6ef4d0p-1 || fabs(cos(1e22) - 0.5232147853951389) < 1e-15);
    ok("pow is exact on integers", pow(3.0, 13.0) == 1594323.0);
    ok("sqrt(-1) is NaN with EDOM", (errno = 0, isnan(sqrt(-1.0)) && errno == EDOM));
    ok("floor, ceil, round, trunc", floor(-1.5) == -2 && ceil(-1.5) == -1 && round(-1.5) == -2 && trunc(-1.5) == -1);
    ok("fmod is exact", fmod(10.0, 3.0) == 1.0 && fmod(-7.5, 2.0) == -1.5);
}

/* ---- stdio --------------------------------------------------------------- */

typedef struct { char data[64]; size_t len; } sink_t;
static ssize_t sink_write(void *c, const char *b, size_t n)
{
    sink_t *s = c;
    size_t k = n < sizeof s->data - 1 - s->len ? n : sizeof s->data - 1 - s->len;
    memcpy(s->data + s->len, b, k);
    s->len += k;
    s->data[s->len] = 0;
    return (ssize_t)n;
}

static void test_stdio(void)
{
    char buf[64], *line = NULL, *mem = NULL;
    size_t cap = 0, msize = 0;
    FILE *f;
    int a = 0, b2 = 0, fd;
    ssize_t n;
    fpos_t pos;

    strcpy(buf, "alpha beta 42\nsecond line\n");
    f = fmemopen(buf, strlen(buf), "r");
    ok("fmemopen opens a buffer for reading", f != NULL);
    if (f) {
        char w[16];
        ok("fscanf reads from a memory stream", fscanf(f, "%15s %*s %d", w, &a) == 2 && strcmp(w, "alpha") == 0 && a == 42);
        fgetc(f);
        n = getline(&line, &cap, f);
        ok("getline returns the next line", n == 12 && strcmp(line, "second line\n") == 0);
        ok("getline at the end returns -1", getline(&line, &cap, f) == -1 && feof(f));
        fclose(f);
    }
    f = fmemopen(buf, 8, "w");
    if (f) {
        fprintf(f, "%d-%s", 123, "abcdefgh");
        fclose(f);
    }
    ok("fmemopen \"w\" stops at the buffer's size", memcmp(buf, "123-abcd", 8) == 0);
    f = open_memstream(&mem, &msize);
    ok("open_memstream", f != NULL);
    if (f) {
        int i;
        for (i = 0; i < 100; i++) fprintf(f, "%d,", i);
        fflush(f);
        ok("open_memstream grows and reports the size on fflush", msize == 290 && strncmp(mem, "0,1,2,", 6) == 0);
        fclose(f);
        ok("open_memstream's buffer is terminated", mem[msize] == 0);
        free(mem);
    }
    {
        static sink_t s;
        cookie_io_functions_t io = { NULL, sink_write, NULL, NULL };
        f = fopencookie(&s, "w", io);
        if (f) { fprintf(f, "cookie %s %d", "works", 1); fclose(f); }
        ok("fopencookie routes writes to the caller's function", strcmp(s.data, "cookie works 1") == 0);
    }
    {
        char *p = NULL;
        ok("asprintf allocates the result", asprintf(&p, "%s-%05.1f", "v", 2.25) == 7 && strcmp(p, "v-002.2") == 0);
        free(p);
    }

    mkdir(D3, 0755);
    fd = open(D3 "/s.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ok("dprintf writes straight to a descriptor", dprintf(fd, "x=%d\n", 5) == 4);
    f = fdopen(fd, "r+");
    ok("fdopen wraps a descriptor", f != NULL);
    if (f) {
        rewind(f);
        ok("fscanf through fdopen", fscanf(f, "x=%d", &b2) == 1 && b2 == 5);
        ok("fgetpos/fsetpos", fgetpos(f, &pos) == 0 && fgetc(f) == '\n' && fsetpos(f, &pos) == 0 && fgetc(f) == '\n');
        ok("ungetc then ftell", ungetc('Q', f) == 'Q' && ftell(f) == 3 && fgetc(f) == 'Q');
        fclose(f);
    }
    f = tmpfile();
    ok("tmpfile is a nameless read/write file", f && fputs("temp", f) >= 0 && fseek(f, 0, SEEK_SET) == 0 &&
       fread(buf, 1, 4, f) == 4 && memcmp(buf, "temp", 4) == 0);
    if (f) fclose(f);
    {
        int have_sh = access("/bin/sh", X_OK) == 0, good = 1;
        if (have_sh) {
            FILE *p = popen("echo piped; exit 3", "r");
            good = p && fgets(buf, sizeof buf, p) && strcmp(buf, "piped\n") == 0;
            if (p) { int st = pclose(p); good = good && WIFEXITED(st) && WEXITSTATUS(st) == 3; }
            good = good && system(NULL) != 0;
            { int st = system("exit 4"); good = good && WIFEXITED(st) && WEXITSTATUS(st) == 4; }
        } else {
            good = system(NULL) == 0;
        }
        ok("popen, pclose and system run /bin/sh where it exists", good);
    }
    free(line);
    unlink(D3 "/s.txt");
}

/* ---- files -------------------------------------------------------------- */

static int not_dot(const struct dirent *e) { return e->d_name[0] != '.'; }

static void test_files(void)
{
    char buf[64], real[PATH_MAX];
    struct stat st;
    struct iovec iov[3];
    int fd, dfd, fl, i;
    struct dirent **list;

    mkdir(D3, 0755);
    fd = open(D3 "/f", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ok("pwrite at an offset", pwrite(fd, "WORLD", 5, 6) == 5 && pwrite(fd, "hello,", 6, 0) == 6);
    ok("pread at an offset leaves the file offset alone", pread(fd, buf, 5, 6) == 5 && memcmp(buf, "WORLD", 5) == 0 &&
       lseek(fd, 0, SEEK_CUR) == 0);
    iov[0].iov_base = (void *)"one "; iov[0].iov_len = 4;
    iov[1].iov_base = (void *)"two "; iov[1].iov_len = 4;
    iov[2].iov_base = (void *)"three"; iov[2].iov_len = 5;
    lseek(fd, 0, SEEK_SET);
    ok("writev gathers", writev(fd, iov, 3) == 13);
    {
        char x[4], y[9];
        iov[0].iov_base = x; iov[0].iov_len = 4;
        iov[1].iov_base = y; iov[1].iov_len = 9;
        lseek(fd, 0, SEEK_SET);
        ok("readv scatters", readv(fd, iov, 2) == 13 && memcmp(x, "one ", 4) == 0 && memcmp(y, "two three", 9) == 0);
    }
    ok("fsync and fdatasync", fsync(fd) == 0 && fdatasync(fd) == 0);
    fl = fcntl(fd, F_GETFL);
    ok("fcntl F_GETFL shows O_RDWR", fl >= 0 && (fl & O_ACCMODE) == O_RDWR);
    ok("fcntl F_SETFL adds O_APPEND", fcntl(fd, F_SETFL, fl | O_APPEND) == 0 && (fcntl(fd, F_GETFL) & O_APPEND));
    ok("fcntl F_SETFD sets close-on-exec", fcntl(fd, F_SETFD, FD_CLOEXEC) == 0 && fcntl(fd, F_GETFD) == FD_CLOEXEC);
    {
        int d = dup3(fd, 30, O_CLOEXEC);
        ok("dup3 with O_CLOEXEC", d == 30 && fcntl(30, F_GETFD) == FD_CLOEXEC);
        close(d);
    }
    close(fd);
    ok("truncate shortens by path", truncate(D3 "/f", 3) == 0 && stat(D3 "/f", &st) == 0 && st.st_size == 3);
    ok("chmod changes the mode", chmod(D3 "/f", 0600) == 0 && stat(D3 "/f", &st) == 0 && (st.st_mode & 0777) == 0600);
    {
        mode_t old = umask(077);
        ok("umask returns the previous mask", umask(old) == 077);
    }
    {
        struct timespec ts[2] = { { 1000000000, 0 }, { 1234567890, 0 } };
        ok("utimensat sets the modification time", utimensat(AT_FDCWD, D3 "/f", ts, 0) == 0 &&
           stat(D3 "/f", &st) == 0 && st.st_mtime == 1234567890);
    }
    ok("symlink creates a link", symlink("f", D3 "/link") == 0);
    {
        ssize_t n = readlink(D3 "/link", buf, sizeof buf);
        ok("readlink returns the target", n == 1 && buf[0] == 'f');
    }
    ok("lstat sees the link, stat follows it", lstat(D3 "/link", &st) == 0 && S_ISLNK(st.st_mode) &&
       stat(D3 "/link", &st) == 0 && S_ISREG(st.st_mode));
    ok("realpath resolves links, dots and dot-dots", realpath(D3 "/./sub/../link", real) == NULL);
    mkdir(D3 "/sub", 0755);
    ok("realpath resolves links, dots and dot-dots (2)", realpath(D3 "/./sub/../link", real) && strcmp(real, D3 "/f") == 0);
    dfd = open(D3, O_RDONLY | O_DIRECTORY);
    ok("mkdirat relative to a directory", mkdirat(dfd, "inner", 0755) == 0);
    fd = openat(dfd, "inner/g", O_WRONLY | O_CREAT, 0644);
    ok("openat relative to a directory", fd >= 0 && write(fd, "gg", 2) == 2);
    close(fd);
    ok("fstatat sees it", fstatat(dfd, "inner/g", &st, 0) == 0 && st.st_size == 2);
    ok("faccessat", faccessat(dfd, "inner/g", R_OK, 0) == 0);
    ok("renameat", renameat(dfd, "inner/g", dfd, "inner/h") == 0 && faccessat(dfd, "inner/h", F_OK, 0) == 0);
    ok("unlinkat removes a file, then the directory", unlinkat(dfd, "inner/h", 0) == 0 &&
       unlinkat(dfd, "inner", AT_REMOVEDIR) == 0);
    close(dfd);
    {
        char t[] = D3 "/tmpXXXXXX";
        fd = mkstemp(t);
        ok("mkstemp creates a unique file", fd >= 0 && strncmp(t, D3 "/tmp", strlen(D3) + 4) == 0 &&
           strchr(t + strlen(D3) + 4, 'X') == NULL);
        if (fd >= 0) { close(fd); unlink(t); }
    }
    {
        char t[] = D3 "/dirXXXXXX";
        ok("mkdtemp creates a directory", mkdtemp(t) != NULL && stat(t, &st) == 0 && S_ISDIR(st.st_mode));
        rmdir(t);
    }
    for (i = 0; i < 3; i++) { char p[64]; snprintf(p, sizeof p, D3 "/sub/%c", "cab"[i]); close(open(p, O_CREAT | O_WRONLY, 0644)); }
    {
        int n = scandir(D3 "/sub", &list, not_dot, alphasort);
        ok("scandir filters and sorts", n == 3 && strcmp(list[0]->d_name, "a") == 0 &&
           strcmp(list[1]->d_name, "b") == 0 && strcmp(list[2]->d_name, "c") == 0);
        while (n > 0) free(list[--n]);
        if (n == 0) free(list);
    }
    {
        DIR *d = opendir(D3 "/sub");
        int first = 0, again = 0;
        struct dirent *e;
        while (d && (e = readdir(d))) first++;
        if (d) rewinddir(d);
        while (d && (e = readdir(d))) again++;
        ok("rewinddir starts over", d && first == 5 && again == 5);
        if (d) closedir(d);
    }
    for (i = 0; i < 3; i++) { char p[64]; snprintf(p, sizeof p, D3 "/sub/%c", "cab"[i]); unlink(p); }
    rmdir(D3 "/sub");
    unlink(D3 "/link");
    unlink(D3 "/f");
}

/* ---- memory ------------------------------------------------------------ */

static void test_memory(void)
{
    char *p;
    int fd;
    void *al;
    p = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ok("mmap anonymous memory, zero-filled", p != MAP_FAILED && p[0] == 0 && p[8191] == 0);
    if (p != MAP_FAILED) {
        p[100] = 'x';
        ok("mprotect to read-only", mprotect(p, 8192, PROT_READ) == 0 && p[100] == 'x');
        ok("munmap", munmap(p, 8192) == 0);
    }
    fd = open(D3 "/m", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ftruncate(fd, 4096);
    p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ok("mmap a file shared", p != MAP_FAILED);
    if (p != MAP_FAILED) {
        char b[8] = { 0 };
        memcpy(p + 10, "mapped", 6);
        msync(p, 4096, MS_SYNC);
        ok("writes through a shared mapping reach the file", pread(fd, b, 6, 10) == 6 && memcmp(b, "mapped", 6) == 0);
        munmap(p, 4096);
    }
    close(fd);
    unlink(D3 "/m");
    al = aligned_alloc(4096, 100);
    ok("aligned_alloc(4096)", al && ((uintptr_t)al & 4095) == 0);
    free(al);
}

/* ---- polling and sockets ---------------------------------------------------- */

static void test_poll_and_sockets(void)
{
    int p[2], sv[2], srv, cli, con;
    struct pollfd pf;
    fd_set rs;
    struct timeval tv = { 0, 0 };
    char buf[32];
    struct sockaddr_in a;
    socklen_t al = sizeof a;

    pipe(p);
    pf.fd = p[0]; pf.events = POLLIN; pf.revents = 0;
    ok("poll on an empty pipe times out", poll(&pf, 1, 0) == 0);
    write(p[1], "z", 1);
    ok("poll sees a readable pipe", poll(&pf, 1, 1000) == 1 && (pf.revents & POLLIN));
    FD_ZERO(&rs); FD_SET(p[0], &rs);
    ok("select sees a readable pipe", select(p[0] + 1, &rs, NULL, NULL, &tv) == 1 && FD_ISSET(p[0], &rs));
    ok("isatty is false for a pipe", isatty(p[0]) == 0 && errno == ENOTTY);
    close(p[0]); close(p[1]);

    ok("socketpair(AF_UNIX)", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    ok("send and recv over a socket pair", send(sv[0], "ping", 4, 0) == 4 && recv(sv[1], buf, sizeof buf, 0) == 4 &&
       memcmp(buf, "ping", 4) == 0);
    ok("shutdown(SHUT_WR) gives the peer end-of-file", shutdown(sv[0], SHUT_WR) == 0 && recv(sv[1], buf, sizeof buf, 0) == 0);
    close(sv[0]); close(sv[1]);

    srv = socket(AF_INET, SOCK_STREAM, 0);
    ok("socket(AF_INET, SOCK_STREAM)", srv >= 0);
    { int one = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one); }
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = 0;
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    ok("bind to 127.0.0.1, any port", bind(srv, (struct sockaddr *)&a, sizeof a) == 0);
    ok("listen", listen(srv, 4) == 0);
    ok("getsockname reports the chosen port", getsockname(srv, (struct sockaddr *)&a, &al) == 0 && ntohs(a.sin_port) != 0);
    cli = socket(AF_INET, SOCK_STREAM, 0);
    ok("connect over the loopback", connect(cli, (struct sockaddr *)&a, sizeof a) == 0);
    con = accept(srv, NULL, NULL);
    ok("accept", con >= 0);
    ok("TCP: client to server", send(cli, "hello tcp", 9, 0) == 9 && recv(con, buf, 9, MSG_WAITALL) == 9 &&
       memcmp(buf, "hello tcp", 9) == 0);
    ok("TCP: server to client", write(con, "back", 4) == 4 && read(cli, buf, sizeof buf) == 4 && memcmp(buf, "back", 4) == 0);
    {
        struct sockaddr_in peer;
        socklen_t pl = sizeof peer;
        char txt[INET_ADDRSTRLEN];
        ok("getpeername is the loopback", getpeername(con, (struct sockaddr *)&peer, &pl) == 0 &&
           inet_ntop(AF_INET, &peer.sin_addr, txt, sizeof txt) && strcmp(txt, "127.0.0.1") == 0);
    }
    close(cli); close(con); close(srv);

    {
        int u1 = socket(AF_INET, SOCK_DGRAM, 0), u2 = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in b, from;
        socklen_t bl = sizeof b, fl = sizeof from;
        memset(&b, 0, sizeof b);
        b.sin_family = AF_INET;
        b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ok("UDP sockets bind", u1 >= 0 && u2 >= 0 && bind(u1, (struct sockaddr *)&b, sizeof b) == 0 &&
           getsockname(u1, (struct sockaddr *)&b, &bl) == 0);
        ok("UDP datagram over the loopback", sendto(u2, "dgram", 5, 0, (struct sockaddr *)&b, sizeof b) == 5 &&
           recvfrom(u1, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl) == 5 && memcmp(buf, "dgram", 5) == 0);
        close(u1); close(u2);
    }
}

/* ---- signals, the system, time, getopt ---------------------------------------- */

static volatile sig_atomic_t got;
static void on_sig(int s) { got = s; }

static void test_system(void)
{
    sigset_t blk, pend, empty;
    struct sigaction sa;
    struct utsname u;
    struct rlimit rl;
    struct timespec res;
    struct timeval tv;
    struct tms t;
    struct rusage ru;
    unsigned char rnd[16];
    char b[64];
    time_t when = 1234567890;
    struct tm tm;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sig;
    sigaction(SIGUSR1, &sa, NULL);
    sigemptyset(&blk); sigaddset(&blk, SIGUSR1);
    sigprocmask(SIG_BLOCK, &blk, NULL);
    raise(SIGUSR1);
    ok("a blocked signal is pending", sigpending(&pend) == 0 && sigismember(&pend, SIGUSR1) == 1 && got == 0);
    sigemptyset(&empty);
    ok("sigsuspend delivers it and returns EINTR", sigsuspend(&empty) == -1 && errno == EINTR && got == SIGUSR1);
    sigaddset(&blk, SIGUSR2);
    sigprocmask(SIG_BLOCK, &blk, NULL);
    raise(SIGUSR2);
    {
        struct timespec zero = { 0, 0 };
        siginfo_t si;
        ok("sigtimedwait collects a pending signal", sigtimedwait(&blk, &si, &zero) == SIGUSR2 && si.si_signo == SIGUSR2);
    }
    sigprocmask(SIG_UNBLOCK, &blk, NULL);
    signal(SIGUSR1, SIG_DFL);

    ok("uname", uname(&u) == 0 && u.sysname[0] != 0);
    ok("gethostname", gethostname(b, sizeof b) == 0);
    ok("getrlimit(RLIMIT_NOFILE)", getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur > 0);
    ok("sysconf(_SC_PAGESIZE) and getauxval(AT_PAGESZ) agree", sysconf(_SC_PAGESIZE) == 4096 && getauxval(AT_PAGESZ) == 4096);
    ok("getrandom", getrandom(rnd, sizeof rnd, 0) == (ssize_t)sizeof rnd);
    ok("arc4random_uniform stays in range", arc4random_uniform(10) < 10 && arc4random_uniform(1) == 0);
    ok("clock_getres(CLOCK_MONOTONIC)", clock_getres(CLOCK_MONOTONIC, &res) == 0 && res.tv_sec == 0 && res.tv_nsec > 0);
    ok("gettimeofday", gettimeofday(&tv, NULL) == 0 && tv.tv_sec > 1600000000);
    ok("times and clock", times(&t) != (clock_t)-1 && clock() >= 0);
    ok("getrusage", getrusage(RUSAGE_SELF, &ru) == 0);
    ok("getuid and geteuid agree", getuid() == geteuid());
    gmtime_r(&when, &tm);
    strftime(b, sizeof b, "%A %d %B %Y, %H:%M:%S (week %V, day %j)", &tm);
    printf("strftime: %s\n", b);
    ok("ctime", strcmp(ctime(&when), "Fri Feb 13 23:31:30 2009\n") == 0);

    {
        char *argv[] = { "prog", "-v", "input", "-o", "out", "--level=3", "--", "-x", NULL };
        static const struct option lo[] = { { "level", required_argument, NULL, 'L' }, { NULL, 0, NULL, 0 } };
        int c, v = 0, level = 0;
        const char *o = NULL;
        optind = 0;
        while ((c = getopt_long(8, argv, "vo:", lo, NULL)) != -1) {
            if (c == 'v') v = 1;
            else if (c == 'o') o = optarg;
            else if (c == 'L') level = atoi(optarg);
        }
        ok("getopt_long parses and permutes", v && o && strcmp(o, "out") == 0 && level == 3 && optind == 6 &&
           strcmp(argv[6], "input") == 0 && strcmp(argv[7], "-x") == 0);
    }
    {
        char *tok, *save, s[] = "a,b;;c";
        int n = 0;
        for (tok = strtok_r(s, ",;", &save); tok; tok = strtok_r(NULL, ",;", &save)) n++;
        ok("strtok_r", n == 3);
        ok("strcasecmp and strerror", strcasecmp("NovaRis", "novaris") == 0 &&
           strcmp(strerror(ECONNREFUSED), "Connection refused") == 0);
    }
    printf("inttypes: %" PRId64 " %" PRIx32 "\n", (int64_t)-9000000000LL, (uint32_t)0xdeadbeefu);
}

void test_v03(void)
{
    printf("novac64: libNovaC 0.3 additions\n");
    test_setjmp();
    test_float();
    test_stdio();
    test_files();
    test_memory();
    test_poll_and_sockets();
    test_system();
    rmdir(D3);
}
