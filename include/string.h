#ifndef NOVAC_STRING_H
#define NOVAC_STRING_H
#include <stddef.h>
#include <novac/defs.h>

NOVAC_BEGIN_DECLS
void  *memcpy(void *restrict dst, const void *restrict src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memchr(const void *s, int c, size_t n);
void  *memrchr(const void *s, int c, size_t n);
void  *memccpy(void *restrict dst, const void *restrict src, int c, size_t n);
void  *mempcpy(void *restrict dst, const void *restrict src, size_t n);
void  *memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen);
void   explicit_bzero(void *p, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
int    strcoll(const char *a, const char *b);
size_t strxfrm(char *restrict dst, const char *restrict src, size_t n);
char  *strcpy(char *restrict dst, const char *restrict src);
char  *strncpy(char *restrict dst, const char *restrict src, size_t n);
char  *stpcpy(char *restrict dst, const char *restrict src);
char  *stpncpy(char *restrict dst, const char *restrict src, size_t n);
/* Always terminate (when n > 0); return the length they tried to create. */
size_t strlcpy(char *restrict dst, const char *restrict src, size_t n);
size_t strlcat(char *restrict dst, const char *restrict src, size_t n);
char  *strcat(char *restrict dst, const char *restrict src);
char  *strncat(char *restrict dst, const char *restrict src, size_t n);
char  *strchr(const char *s, int c);
char  *strchrnul(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);
char  *strcasestr(const char *haystack, const char *needle);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char  *strpbrk(const char *s, const char *accept);
char  *strtok(char *restrict s, const char *restrict delim);
char  *strtok_r(char *restrict s, const char *restrict delim, char **restrict save);
char  *strsep(char **sp, const char *delim);
char  *strdup(const char *s);
char  *strndup(const char *s, size_t n);

char  *strerror(int errnum);
/* The POSIX (XSI) form: 0 on success, ERANGE if the message was cut short. */
int    strerror_r(int errnum, char *buf, size_t n);
char  *strsignal(int sig);
NOVAC_END_DECLS
#endif
