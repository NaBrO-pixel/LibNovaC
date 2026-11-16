/* opendir/readdir/closedir over getdents64. */
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "../internal/syscall.h"

_Static_assert(offsetof(struct dirent, d_name) == 19, "dirent must match the Linux getdents64 layout");

struct novac_dir {
    int fd;
    size_t pos, len;
    long eof;
    long tell;                     /* d_off of the last entry returned (0 = the start) */
    char buf[1024] __attribute__((aligned(8)));
};

DIR *fdopendir(int fd)
{
    DIR *d;
    struct stat st;
    if (fstat(fd, &st) < 0) return NULL;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return NULL; }
    d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->fd = fd;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return d;
}

DIR *opendir(const char *path)
{
    DIR *d;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return NULL;
    d = calloc(1, sizeof *d);
    if (!d) { int e = errno; close(fd); errno = e; return NULL; }
    d->fd = fd;
    return d;
}

int dirfd(DIR *d) { return d->fd; }

void seekdir(DIR *d, long pos)
{
    if (lseek(d->fd, pos, SEEK_SET) < 0) return;
    d->pos = d->len = 0;
    d->eof = 0;
    d->tell = pos;
}
void rewinddir(DIR *d) { seekdir(d, 0); }
long telldir(DIR *d)   { return d->tell; }

int alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcoll((*a)->d_name, (*b)->d_name);
}

int scandir(const char *path, struct dirent ***list, int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **))
{
    DIR *d = opendir(path);
    struct dirent *e, **v = NULL;
    size_t n = 0, cap = 0;
    int saved;
    if (!d) return -1;
    errno = 0;
    while ((e = readdir(d))) {
        struct dirent *copy;
        if (filter && !filter(e)) continue;
        if (n == cap) {
            struct dirent **nv;
            cap = cap ? cap * 2 : 16;
            nv = realloc(v, cap * sizeof *v);
            if (!nv) goto fail;
            v = nv;
        }
        copy = malloc(e->d_reclen > sizeof *e ? e->d_reclen : sizeof *e);
        if (!copy) goto fail;
        memcpy(copy, e, e->d_reclen < sizeof *e ? e->d_reclen : sizeof *e);
        v[n++] = copy;
    }
    if (errno) goto fail;
    closedir(d);
    if (compar && n > 1) qsort(v, n, sizeof *v, (int (*)(const void *, const void *))compar);
    *list = v;
    return (int)n;
fail:
    saved = errno ? errno : ENOMEM;
    while (n) free(v[--n]);
    free(v);
    closedir(d);
    errno = saved;
    return -1;
}

struct dirent *readdir(DIR *d)
{
    struct dirent *e;
    if (d->pos >= d->len) {
        long n;
        if (d->eof) return NULL;
        n = novac_sys3(NOVAC_SYS_getdents64, d->fd, d->buf, sizeof d->buf);
        if (n < 0) return NULL;                 /* errno set by the wrapper */
        if (n == 0) { d->eof = 1; return NULL; }
        d->pos = 0;
        d->len = (size_t)n;
    }
    e = (struct dirent *)(d->buf + d->pos);
    if (e->d_reclen == 0 || d->pos + e->d_reclen > d->len) { errno = EIO; return NULL; }
    d->pos += e->d_reclen;
    d->tell = e->d_off;
    return e;
}

int closedir(DIR *d)
{
    int rc;
    if (!d) { errno = EINVAL; return -1; }
    rc = close(d->fd);
    free(d);
    return rc;
}
