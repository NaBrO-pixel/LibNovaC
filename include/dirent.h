#ifndef NOVAC_DIRENT_H
#define NOVAC_DIRENT_H
#include <sys/types.h>
#include <novac/defs.h>

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12

/* Linux getdents64 record layout. */
struct dirent {
    ino_t d_ino;
    off_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

typedef struct novac_dir DIR;

NOVAC_BEGIN_DECLS
DIR *opendir(const char *path);
/* Returns NULL at the end of the directory (errno unchanged) or on error
 * (errno set). The result is valid until the next readdir/closedir. */
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
DIR *fdopendir(int fd);
int  dirfd(DIR *d);
void rewinddir(DIR *d);
long telldir(DIR *d);
void seekdir(DIR *d, long pos);
/* Reads a whole directory: entries `filter` accepts, sorted by `compar`;
 * returns the count and an array the caller frees (each entry and the
 * array), or -1. */
int  scandir(const char *path, struct dirent ***list, int (*filter)(const struct dirent *),
             int (*compar)(const struct dirent **, const struct dirent **));
int  alphasort(const struct dirent **a, const struct dirent **b);
NOVAC_END_DECLS
#endif
