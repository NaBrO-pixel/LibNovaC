/* <libgen.h>: POSIX basename and dirname. */
#include <libgen.h>
#include <string.h>

char *basename(char *path)
{
    size_t n;
    if (!path || !*path) return (char *)".";
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/') path[--n] = 0;   /* "a/b//" -> "a/b" */
    if (n == 1 && path[0] == '/') return path;
    { char *slash = strrchr(path, '/'); return slash ? slash + 1 : path; }
}

char *dirname(char *path)
{
    size_t n;
    if (!path || !*path) return (char *)".";
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/') n--;            /* trailing slashes */
    while (n > 0 && path[n - 1] != '/') n--;            /* the last component */
    if (n == 0) return (char *)".";
    while (n > 1 && path[n - 1] == '/') n--;            /* slashes before it */
    path[n] = 0;
    return path;
}
