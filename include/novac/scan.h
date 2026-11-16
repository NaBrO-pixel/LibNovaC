/* Reusable scanning engine behind the scanf family. Input comes from a
 * caller-supplied source with one character of push-back, so the same code
 * serves strings (sscanf) and FILE streams (fscanf). */
#ifndef NOVAC_SCAN_H
#define NOVAC_SCAN_H
#include <stdarg.h>
#include <novac/defs.h>

NOVAC_BEGIN_DECLS
typedef struct {
    int  (*get)(void *ctx);            /* next byte as unsigned char, or -1 at end */
    void (*unget)(int c, void *ctx);   /* push back the byte just read */
    void *ctx;
} novac_scan_src;

/* Returns the number of assigned items, or -1 (EOF) if input ended before
 * the first conversion completed. */
int novac_scan(const novac_scan_src *src, const char *fmt, va_list ap);
NOVAC_END_DECLS
#endif
