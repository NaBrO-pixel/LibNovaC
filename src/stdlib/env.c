/* getenv, setenv, unsetenv over `environ` (set up by crt0). Portable: no
 * system calls. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

char *getenv(const char *name)
{
    size_t n;
    char **e;
    if (!environ || !name || !*name) return NULL;
    for (n = 0; name[n]; n++)
        if (name[n] == '=') return NULL;
    for (e = environ; *e; e++) {
        size_t i = 0;
        while (i < n && (*e)[i] == name[i]) i++;
        if (i == n && (*e)[n] == '=') return *e + n + 1;
    }
    return NULL;
}

/* --- the environment ----------------------------------------------------- *
 *
 * environ starts as the array the kernel left on the stack. The first
 * change copies it into memory of our own, which then grows as needed. A
 * string replaced or removed is not freed - it may be one of the
 * kernel's, or one a caller still holds - which is the usual trade. */
static char **owned_env;
static size_t owned_cap;

static int env_name_ok(const char *name)
{
    if (!name || !*name) return 0;
    for (; *name; name++) if (*name == '=') return 0;
    return 1;
}

static long env_find(const char *name, size_t n)
{
    long i;
    if (!environ) return -1;
    for (i = 0; environ[i]; i++)
        if (strncmp(environ[i], name, n) == 0 && environ[i][n] == '=') return i;
    return -1;
}

static size_t env_count(void)
{
    size_t c = 0;
    if (environ) while (environ[c]) c++;
    return c;
}

static int env_own(size_t need)
{
    size_t count = env_count(), cap, i;
    char **fresh;
    if (environ == owned_env && need + 1 <= owned_cap) return 0;
    cap = owned_cap ? owned_cap : 16;
    while (cap < need + 1) cap *= 2;
    fresh = malloc(cap * sizeof *fresh);
    if (!fresh) { errno = ENOMEM; return -1; }
    for (i = 0; i < count; i++) fresh[i] = environ[i];
    fresh[count] = NULL;
    if (environ == owned_env) free(owned_env);
    owned_env = environ = fresh;
    owned_cap = cap;
    return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
    size_t n, v;
    long at;
    char *entry;

    if (!env_name_ok(name)) { errno = EINVAL; return -1; }
    if (!value) value = "";
    n = strlen(name);
    at = env_find(name, n);
    if (at >= 0 && !overwrite) return 0;
    v = strlen(value);
    entry = malloc(n + 1 + v + 1);
    if (!entry) { errno = ENOMEM; return -1; }
    memcpy(entry, name, n);
    entry[n] = '=';
    memcpy(entry + n + 1, value, v + 1);
    if (env_own(env_count() + (at < 0 ? 1 : 0)) < 0) { free(entry); return -1; }
    if (at >= 0) {
        environ[at] = entry;
    } else {
        size_t c = env_count();
        environ[c] = entry;
        environ[c + 1] = NULL;
    }
    return 0;
}

int unsetenv(const char *name)
{
    size_t n;
    long at;
    if (!env_name_ok(name)) { errno = EINVAL; return -1; }
    n = strlen(name);
    while ((at = env_find(name, n)) >= 0) {
        if (env_own(env_count()) < 0) return -1;
        for (; environ[at]; at++) environ[at] = environ[at + 1];
    }
    return 0;
}

int clearenv(void)
{
    if (environ == owned_env && owned_env) owned_env[0] = NULL;
    else environ = NULL;
    return 0;
}
