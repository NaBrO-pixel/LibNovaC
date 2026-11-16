#ifndef NOVAC_SYS_WAIT_H
#define NOVAC_SYS_WAIT_H
#include <sys/types.h>
#include <novac/defs.h>

#define WNOHANG    1
#define WUNTRACED  2
#define WSTOPPED   WUNTRACED
#define WCONTINUED 8

/* Linux wait-status encoding. */
#define WEXITSTATUS(s)  (((s) >> 8) & 0xff)
#define WTERMSIG(s)     ((s) & 0x7f)
#define WSTOPSIG(s)     WEXITSTATUS(s)
#define WIFEXITED(s)    (WTERMSIG(s) == 0)
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#define WIFSIGNALED(s)  (((s) & 0xffff) - 1U < 0xffU)
#define WIFCONTINUED(s) ((s) == 0xffff)
#define WCOREDUMP(s)    ((s) & 0x80)

NOVAC_BEGIN_DECLS
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
NOVAC_END_DECLS
#endif
