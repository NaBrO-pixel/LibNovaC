/* Formatting into memory (portable) and buffered FILE streams (system
 * layer; needs the NovarisOS/Linux syscall binding). */
#ifndef NOVAC_STDIO_H
#define NOVAC_STDIO_H
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#include <novac/defs.h>

#define EOF      (-1)
#define BUFSIZ   1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 1024
#define L_tmpnam 20
#define TMP_MAX  238328
#define P_tmpdir "/tmp"
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

typedef struct novac_file FILE;
typedef struct { off_t __pos; } fpos_t;
extern FILE *novac_stdin, *novac_stdout, *novac_stderr;
#define stdin  novac_stdin
#define stdout novac_stdout
#define stderr novac_stderr

/* GNU fopencookie: a stream over your own read/write/seek/close. */
typedef ssize_t cookie_read_function_t(void *cookie, char *buf, size_t n);
typedef ssize_t cookie_write_function_t(void *cookie, const char *buf, size_t n);
typedef int     cookie_seek_function_t(void *cookie, off_t *off, int whence);
typedef int     cookie_close_function_t(void *cookie);
typedef struct {
    cookie_read_function_t  *read;
    cookie_write_function_t *write;
    cookie_seek_function_t  *seek;
    cookie_close_function_t *close;
} cookie_io_functions_t;

NOVAC_BEGIN_DECLS
/* ---- formatting into memory (portable) ---- */
int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...)
    NOVAC_PRINTF_LIKE(3, 4);
int vsnprintf(char *restrict buf, size_t n, const char *restrict fmt, va_list ap)
    NOVAC_PRINTF_LIKE(3, 0);
/* Unbounded by definition; prefer snprintf. Writes at most INT_MAX bytes. */
int sprintf(char *restrict buf, const char *restrict fmt, ...)
    NOVAC_PRINTF_LIKE(2, 3);
int vsprintf(char *restrict buf, const char *restrict fmt, va_list ap)
    NOVAC_PRINTF_LIKE(2, 0);
/* Allocate the result (free() it); -1 and *out unspecified on failure. */
int asprintf(char **restrict out, const char *restrict fmt, ...) NOVAC_PRINTF_LIKE(2, 3);
int vasprintf(char **restrict out, const char *restrict fmt, va_list ap) NOVAC_PRINTF_LIKE(2, 0);
int sscanf(const char *restrict s, const char *restrict fmt, ...) NOVAC_SCANF_LIKE(2, 3);
int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap) NOVAC_SCANF_LIKE(2, 0);

/* ---- streams ---- */
FILE  *fopen(const char *restrict path, const char *restrict mode);
FILE  *fdopen(int fd, const char *mode);
FILE  *freopen(const char *restrict path, const char *restrict mode, FILE *restrict f);
FILE  *fopencookie(void *cookie, const char *mode, cookie_io_functions_t io);
/* A stream over a caller's buffer (or an internal one, if buf is NULL). */
FILE  *fmemopen(void *restrict buf, size_t size, const char *restrict mode);
/* A write stream into a growing buffer: *buf and *size are updated on each
 * fflush and on fclose; free(*buf) afterwards. */
FILE  *open_memstream(char **buf, size_t *size);
FILE  *tmpfile(void);
/* Runs "/bin/sh -c cmd"; mode "r" or "w" (plus 'e' for close-on-exec). */
FILE  *popen(const char *cmd, const char *mode);
int    pclose(FILE *f);
int    fclose(FILE *f);
int    fflush(FILE *f);            /* fflush(NULL) flushes every open stream */
size_t fread(void *restrict buf, size_t size, size_t n, FILE *restrict f);
size_t fwrite(const void *restrict buf, size_t size, size_t n, FILE *restrict f);
int    fgetc(FILE *f);
int    getc(FILE *f);
int    getchar(void);
int    ungetc(int c, FILE *f);
char  *fgets(char *restrict buf, int n, FILE *restrict f);
ssize_t getline(char **restrict line, size_t *restrict cap, FILE *restrict f);
ssize_t getdelim(char **restrict line, size_t *restrict cap, int delim, FILE *restrict f);
int    fputc(int c, FILE *f);
int    putc(int c, FILE *f);
int    putchar(int c);
int    fputs(const char *restrict s, FILE *restrict f);
int    puts(const char *s);
int    getc_unlocked(FILE *f);
int    getchar_unlocked(void);
int    putc_unlocked(int c, FILE *f);
int    putchar_unlocked(int c);
void   flockfile(FILE *f);
int    ftrylockfile(FILE *f);
void   funlockfile(FILE *f);
int    fseek(FILE *f, long off, int whence);
int    fseeko(FILE *f, off_t off, int whence);
long   ftell(FILE *f);
off_t  ftello(FILE *f);
int    fgetpos(FILE *restrict f, fpos_t *restrict pos);
int    fsetpos(FILE *f, const fpos_t *pos);
void   rewind(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   clearerr(FILE *f);
int    fileno(FILE *f);
int    setvbuf(FILE *restrict f, char *restrict buf, int mode, size_t size);
void   setbuf(FILE *restrict f, char *restrict buf);
void   setlinebuf(FILE *f);
int    remove(const char *path);
int    rename(const char *from, const char *to);
int    renameat(int fromdir, const char *from, int todir, const char *to);
void   perror(const char *s);
int    printf(const char *restrict fmt, ...) NOVAC_PRINTF_LIKE(1, 2);
int    vprintf(const char *restrict fmt, va_list ap) NOVAC_PRINTF_LIKE(1, 0);
int    fprintf(FILE *restrict f, const char *restrict fmt, ...) NOVAC_PRINTF_LIKE(2, 3);
int    vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap) NOVAC_PRINTF_LIKE(2, 0);
int    dprintf(int fd, const char *restrict fmt, ...) NOVAC_PRINTF_LIKE(2, 3);
int    vdprintf(int fd, const char *restrict fmt, va_list ap) NOVAC_PRINTF_LIKE(2, 0);
int    scanf(const char *restrict fmt, ...) NOVAC_SCANF_LIKE(1, 2);
int    vscanf(const char *restrict fmt, va_list ap) NOVAC_SCANF_LIKE(1, 0);
int    fscanf(FILE *restrict f, const char *restrict fmt, ...) NOVAC_SCANF_LIKE(2, 3);
int    vfscanf(FILE *restrict f, const char *restrict fmt, va_list ap) NOVAC_SCANF_LIKE(2, 0);
NOVAC_END_DECLS
#endif
