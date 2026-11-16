/* Build-time check (host headers): every syscall number libNovaC uses must
 * equal the kernel's definition in include/syscall64.h. */
#include "syscall64.h"
#include "../../src/arch/x86_64/sysnum.h"

#define SAME(name, kname) _Static_assert(NOVAC_SYS_##name == kname, #name " differs from the kernel")
SAME(read, SYS64_READ);           SAME(write, SYS64_WRITE);
SAME(open, SYS64_OPEN);           SAME(close, SYS64_CLOSE);
SAME(stat, SYS64_STAT);           SAME(fstat, SYS64_FSTAT);
SAME(lstat, SYS64_LSTAT);         SAME(lseek, SYS64_LSEEK);
SAME(mmap, SYS64_MMAP);           SAME(munmap, SYS64_MUNMAP);
SAME(access, SYS64_ACCESS);       SAME(dup, SYS64_DUP);
SAME(dup2, SYS64_DUP2);           SAME(nanosleep, SYS64_NANOSLEEP);
SAME(getpid, SYS64_GETPID);       SAME(ftruncate, SYS64_FTRUNCATE);
SAME(getcwd, SYS64_GETCWD);       SAME(chdir, SYS64_CHDIR);
SAME(rename, SYS64_RENAME);       SAME(mkdir, SYS64_MKDIR);
SAME(rmdir, SYS64_RMDIR);         SAME(unlink, SYS64_UNLINK);
SAME(gettimeofday, SYS64_GETTIMEOFDAY);
SAME(clock_gettime, SYS64_CLOCK_GETTIME);
SAME(exit_group, SYS64_EXIT_GROUP);
SAME(pipe2, SYS64_PIPE2);         SAME(fork, SYS64_FORK);
SAME(wait4, SYS64_WAIT4);         SAME(getppid, SYS64_GETPPID);
SAME(getdents64, SYS64_GETDENTS64);
SAME(rt_sigaction, SYS64_RT_SIGACTION); SAME(rt_sigprocmask, SYS64_RT_SIGPROCMASK);
SAME(rt_sigreturn, SYS64_RT_SIGRETURN); SAME(execve, SYS64_EXECVE);
SAME(kill, SYS64_KILL);           SAME(gettid, SYS64_GETTID);
SAME(tgkill, SYS64_TGKILL);
SAME(poll, SYS64_POLL); SAME(mprotect, SYS64_MPROTECT);
SAME(ioctl, SYS64_IOCTL); SAME(pread64, SYS64_PREAD64);
SAME(pwrite64, SYS64_PWRITE64); SAME(readv, SYS64_READV);
SAME(writev, SYS64_WRITEV); SAME(sched_yield, SYS64_SCHED_YIELD);
SAME(msync, SYS64_MSYNC); SAME(madvise, SYS64_MADVISE);
SAME(socket, SYS64_SOCKET); SAME(connect, SYS64_CONNECT);
SAME(accept, SYS64_ACCEPT); SAME(sendto, SYS64_SENDTO);
SAME(recvfrom, SYS64_RECVFROM); SAME(sendmsg, SYS64_SENDMSG);
SAME(recvmsg, SYS64_RECVMSG); SAME(shutdown, SYS64_SHUTDOWN);
SAME(bind, SYS64_BIND); SAME(listen, SYS64_LISTEN);
SAME(getsockname, SYS64_GETSOCKNAME); SAME(getpeername, SYS64_GETPEERNAME);
SAME(socketpair, SYS64_SOCKETPAIR); SAME(setsockopt, SYS64_SETSOCKOPT);
SAME(getsockopt, SYS64_GETSOCKOPT); SAME(uname, SYS64_UNAME);
SAME(fcntl, SYS64_FCNTL); SAME(fsync, SYS64_FSYNC);
SAME(fdatasync, SYS64_FDATASYNC); SAME(truncate, SYS64_TRUNCATE);
SAME(fchdir, SYS64_FCHDIR); SAME(symlink, SYS64_SYMLINK);
SAME(readlink, SYS64_READLINK); SAME(chmod, SYS64_CHMOD);
SAME(fchmod, SYS64_FCHMOD); SAME(umask, SYS64_UMASK);
SAME(getrusage, SYS64_GETRUSAGE); SAME(sysinfo, SYS64_SYSINFO);
SAME(times, SYS64_TIMES); SAME(getuid, SYS64_GETUID);
SAME(getgid, SYS64_GETGID); SAME(geteuid, SYS64_GETEUID);
SAME(getegid, SYS64_GETEGID); SAME(setsid, SYS64_SETSID);
SAME(rt_sigpending, SYS64_RT_SIGPENDING); SAME(rt_sigtimedwait, SYS64_RT_SIGTIMEDWAIT);
SAME(rt_sigsuspend, SYS64_RT_SIGSUSPEND); SAME(sigaltstack, SYS64_SIGALTSTACK);
SAME(statfs, SYS64_STATFS); SAME(fstatfs, SYS64_FSTATFS);
SAME(clock_getres, SYS64_CLOCK_GETRES); SAME(clock_nanosleep, SYS64_CLOCK_NANOSLEEP);
SAME(openat, SYS64_OPENAT); SAME(mkdirat, SYS64_MKDIRAT);
SAME(newfstatat, SYS64_NEWFSTATAT); SAME(unlinkat, SYS64_UNLINKAT);
SAME(renameat, SYS64_RENAMEAT); SAME(symlinkat, SYS64_SYMLINKAT);
SAME(readlinkat, SYS64_READLINKAT); SAME(faccessat, SYS64_FACCESSAT);
SAME(pselect6, SYS64_PSELECT6); SAME(ppoll, SYS64_PPOLL);
SAME(utimensat, SYS64_UTIMENSAT); SAME(accept4, SYS64_ACCEPT4);
SAME(dup3, SYS64_DUP3); SAME(prlimit64, SYS64_PRLIMIT64);
SAME(getrandom, SYS64_GETRANDOM); SAME(exit, SYS64_EXIT);
int novac_sysnum_checked;
