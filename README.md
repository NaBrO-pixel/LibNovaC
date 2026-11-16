# libNovaC

NovarisOS's own C library: a freestanding C11 library for static x86-64
programs that run on NovarisOS. Programs linked against it need no host
libc at all. libNovaC brings its own start-up code (`crt0.o`), its own
allocator, stdio and math, and talks to the kernel through raw system calls.

NovarisOS implements the Linux x86-64 system-call ABI, so a libNovaC
program is also an ordinary static Linux executable. That is how it is
tested: the same binary runs on the build host and inside NovarisOS, and
the two transcripts have to match.

Version 0.3. See [CHANGELOG.md](CHANGELOG.md) for what changed.

## What it provides

| Header | Contents |
| --- | --- |
| `<stdio.h>` | buffered `FILE` streams over descriptors or your own functions (`fopencookie`), `fmemopen`, `open_memstream`, `tmpfile`, `popen`; the whole `printf` family including floating point, `asprintf`, `dprintf`; the `scanf` family; `getline`/`getdelim`; `fseeko`/`ftello`/`fgetpos` |
| `<stdlib.h>` | `malloc` family plus `aligned_alloc`/`posix_memalign`/`reallocarray`; `strtod`/`strtof` (correctly rounded) and the integer `strto*`; `qsort`/`qsort_r`/`bsearch`; `rand`, `random`, the `drand48` family, `arc4random`; `getenv`/`setenv`/`unsetenv`/`clearenv`; `system`, `mkstemp`/`mkdtemp`, `realpath`; `exit`/`atexit`/`quick_exit`/`abort` |
| `<string.h>`, `<strings.h>` | the `mem*`/`str*` functions, `strtok_r`, `strsep`, `strspn`/`strcspn`/`strpbrk`, `strdup`/`strndup`, `stpcpy`, `strlcpy`/`strlcat`, `memmem`, `memrchr`, `strcasecmp`, `strerror`/`strerror_r`, `strsignal`, `ffs` |
| `<math.h>`, `<fenv.h>` | exp/log families, `pow`, trigonometric, inverse and hyperbolic functions, `cbrt`, `hypot`, rounding, `fmod`/`remainder`/`remquo`, `frexp`/`ldexp`/`scalbn`, classification macros; float and long double forms; rounding modes and exception flags |
| `<setjmp.h>` | `setjmp`/`longjmp`, `sigsetjmp`/`siglongjmp` |
| `<signal.h>` | `sigaction` (with `SA_SIGINFO`), `signal`, `raise`, `kill`, `sigprocmask`, `sigsuspend`, `sigpending`, `sigtimedwait`/`sigwait`, `sigaltstack`, `psignal` |
| `<unistd.h>`, `<fcntl.h>`, `<sys/stat.h>` | I/O including `pread`/`pwrite`, `fcntl`, the `*at()` calls, links, `chmod`/`umask`, `utimensat`, `truncate`, processes (`fork`, the `exec` family, `pause`), identity, `isatty`, `sysconf`, `getopt` |
| `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<netdb.h>`, `<sys/un.h>` | BSD sockets (AF_UNIX, AF_INET TCP and UDP), `inet_pton`/`inet_ntop`/`inet_aton`, numeric `getaddrinfo`/`getnameinfo` |
| `<poll.h>`, `<sys/select.h>` | `poll`, `ppoll`, `select`, `pselect` |
| `<sys/mman.h>`, `<sys/uio.h>` | `mmap`, `munmap`, `mprotect`, `msync`, `madvise`; `readv`/`writev` |
| `<time.h>`, `<sys/time.h>`, `<sys/times.h>` | clocks, `nanosleep`/`clock_nanosleep`, `clock`, `gmtime`/`localtime`/`timegm`/`mktime` (UTC), `strftime`, `asctime`/`ctime`, `gettimeofday`, `times` |
| `<dirent.h>` | `opendir`/`fdopendir`/`readdir`/`rewinddir`/`seekdir`, `scandir`/`alphasort` |
| `<sys/resource.h>`, `<sys/utsname.h>`, `<sys/random.h>`, `<sys/auxv.h>`, `<sched.h>` | `getrlimit`/`setrlimit`/`prlimit`, `getrusage`, `uname`, `getrandom`/`getentropy`, `getauxval`, `sched_yield` |
| `<termios.h>`, `<sys/ioctl.h>` | `tcgetattr`/`tcsetattr`, `cfmakeraw`, speeds, `ioctl` |
| `<getopt.h>`, `<fnmatch.h>`, `<libgen.h>`, `<inttypes.h>`, `<locale.h>` | `getopt_long`, shell wildcards, `basename`/`dirname`, `PRI*`/`SCN*`, `strtoimax`, the "C" locale |
| `<ctype.h>`, `<errno.h>`, `<limits.h>`, `<assert.h>`, `<sys/types.h>`, `<sys/wait.h>` | as expected; errno values (all of Linux's) and messages are Linux's |

## Accuracy

The conversions between binary and decimal are exact:

- **printf.** `%f`, `%e` and `%g` are printed from the exact decimal
  expansion of the double, rounded ties-to-even, so they match glibc digit
  for digit.
- **strtod and strtof.** Both are correctly rounded for any input length,
  in decimal and in hex.

The double-precision math functions use double-double kernels. Measured
against glibc on 40,000 random arguments per function, every result is
within 1 ulp of glibc's. Functions where glibc itself is less accurate
(`log10`, `cbrt`, `sinh`, `cosh`, `tanh`) differ from it by 2 or 3 ulps,
and in the cases checked against high-precision arithmetic it was libNovaC
that was right. The float forms are rounded from the double results.
On every argument tested they equal glibc's double result rounded to
float.

## Layout

- **`src/`** is the library.
  - **The portable core** (`string`, `ctype`, `stdlib`, `fp`, `math`,
    `time/calendar.c`, formatting and scanning, `inet`, `getopt`) reaches
    the kernel only through the four hooks in `src/internal/sys.h`: map,
    unmap, write to stderr and exit. `src/sys/novaris/sys.c` implements them.
  - **Everything else that talks to the kernel** goes through
    `src/internal/syscall.h`, the one place a `-errno` becomes `-1` and
    `errno`.
- **`src/arch/x86_64/`** holds `_start`, the `syscall` instruction, and
  the syscall numbers. `make check-sysnum` compares those numbers with the
  kernel's `include/syscall64.h`, in one of three places:
  - when built as `userland/libnovac`, the enclosing NovarisOS tree's copy;
  - otherwise, the vendored copy in `third_party/novaris/`;
  - with `NOVARIS_INC=/path/to/NovarisOS/include`, the header at that path.
- **`tests/host/`** holds unit tests of the portable core. They run on the
  host under ASan and UBSan, with symbols renamed so they cannot clash with
  the host libc. Wherever glibc can serve as a reference, they compare
  against it on random inputs (`oracle.c`).
- **`tests/target/`** holds the target test and `novac_child`, the program
  it execs. The target test is a static program that uses only libNovaC.
- **`tools/`** holds the generators for `errno.h` and the message table,
  the math constant tables (computed with Python's `decimal`), and the host
  tests' `rename.h`.

## Building and testing

```bash
make host-test      # portable-core unit tests on the host (ASan + UBSan)
make lib            # build/target/libNovaC.a and crt0.o
make target-test    # link check, then the target test on this x86-64 Linux host
make check-sysnum   # syscall numbers against NovarisOS's syscall64.h
make example        # build/target/examples/demo, a small static program
```

To link a program against it:

```bash
gcc -std=c11 -ffreestanding -fno-builtin -nostdinc -isystem include \
    -isystem "$(gcc -print-file-name=include)" -c prog.c
ld -m elf_x86_64 -static -nostdlib -e _start -o prog \
    build/target/crt0.o prog.o build/target/libNovaC.a
```

After adding a public function, run `python3 tools/gen_rename.py`.

## Limits

- **Threads.** There are none. `errno` and the stdio state are
  process-wide.
- **Locales and wide characters.** Only the "C" locale exists, and there
  is no `<wchar.h>`.
- **Time zones.** Local time is UTC.
- **Name resolution.** `getaddrinfo` takes numeric addresses and
  `localhost` only.
- **long double.** Values are printed, parsed and computed at double
  precision.
- **Math.** `fma`, `erf`, `erfc`, `tgamma` and `lgamma` are not provided.
  The math functions assume the default round-to-nearest mode.
- **stdout buffering.** stdout is always line-buffered. There is no
  switch to full buffering when it is not a terminal, so transcripts keep
  their order around `fork`.

## In NovarisOS

NovarisOS's `Makefile.amd64` uses this library as follows:

1. It builds libNovaC with this Makefile into `build64/libnovac`, after
   checking the syscall numbers.
2. It embeds the target test in the kernel and stages `novac_child` at
   `/tmp/novac_child` in the initrd.
3. The bring-up runs the target test as the "libNovaC" layer and expects
   exit status 131.
4. `make -f Makefile.amd64 test` then runs the same binary on the host and
   requires every line it prints there to appear in the guest's transcript
   too.
