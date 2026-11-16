/* fnmatch: shell wildcard matching. Iterative, with the classic single
 * backtrack point for '*' (a later '*' supersedes an earlier one), so it
 * runs in O(pattern * string) and cannot blow the stack. With
 * FNM_PATHNAME, '*' and '?' never match '/', and the backtrack is
 * confined to one path component. */
#include <fnmatch.h>
#include <ctype.h>
#include <string.h>

static int fold(int c, int flags) { return (flags & FNM_CASEFOLD) ? tolower(c) : c; }

static int class_match(const char *name, size_t n, int c)
{
#define CL(s, f) if (n == sizeof s - 1 && memcmp(name, s, n) == 0) return f(c) != 0;
    CL("alnum", isalnum) CL("alpha", isalpha) CL("blank", isblank) CL("cntrl", iscntrl)
    CL("digit", isdigit) CL("graph", isgraph) CL("lower", islower) CL("print", isprint)
    CL("punct", ispunct) CL("space", isspace) CL("upper", isupper) CL("xdigit", isxdigit)
#undef CL
    return -1;
}

/* Matches one character against the bracket expression at *pp (just past
 * '['). Returns 1/0, or -1 if the bracket is malformed (then '[' is literal). */
static int bracket(const char **pp, int c, int flags)
{
    const char *p = *pp;
    int neg = 0, ok = 0, first = 1;
    if (*p == '!' || *p == '^') { neg = 1; p++; }
    while (*p && (*p != ']' || first)) {
        int lo, hi;
        first = 0;
        if (p[0] == '[' && p[1] == ':') {
            const char *end = strstr(p + 2, ":]");
            if (end) {
                int r = class_match(p + 2, (size_t)(end - p - 2), c);
                if (r >= 0) { if (r) ok = 1; p = end + 2; continue; }
            }
        }
        if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1]) p++;
        lo = (unsigned char)*p++;
        hi = lo;
        if (p[0] == '-' && p[1] && p[1] != ']') {
            p++;
            if (*p == '\\' && !(flags & FNM_NOESCAPE) && p[1]) p++;
            hi = (unsigned char)*p++;
        }
        if ((flags & FNM_PATHNAME) && c == '/') continue;
        if ((lo <= c && c <= hi) ||
            ((flags & FNM_CASEFOLD) && ((lo <= tolower(c) && tolower(c) <= hi) || (lo <= toupper(c) && toupper(c) <= hi))))
            ok = 1;
    }
    if (*p != ']') return -1;
    *pp = p + 1;
    if ((flags & FNM_PATHNAME) && c == '/') return 0;
    return ok != neg;
}

static int leading_period(const char *s, const char *start, int flags)
{
    if (!(flags & FNM_PERIOD) || *s != '.') return 0;
    return s == start || ((flags & FNM_PATHNAME) && s[-1] == '/');
}

int fnmatch(const char *pat, const char *str, int flags)
{
    const char *p = pat, *s = str, *star_p = NULL, *star_s = NULL;
    for (;;) {
        int c = (unsigned char)*s;
        if (*p == '*') {
            while (*p == '*') p++;
            if (leading_period(s, str, flags)) goto fail;
            if (!*p) {
                if (!(flags & FNM_PATHNAME)) return 0;
                if (flags & FNM_LEADING_DIR) return 0;
                return strchr(s, '/') ? FNM_NOMATCH : 0;
            }
            star_p = p; star_s = s;
            continue;
        }
        if (!c) {
            if (!*p) return 0;
            if ((flags & FNM_LEADING_DIR) && 0) return 0;
            goto fail;
        }
        if (!*p) {
            if ((flags & FNM_LEADING_DIR) && c == '/') return 0;
            goto fail;
        }
        if (*p == '?') {
            if ((flags & FNM_PATHNAME) && c == '/') goto fail;
            if (leading_period(s, str, flags)) goto fail;
            p++; s++;
            continue;
        }
        if (*p == '[') {
            const char *q = p + 1;
            int r;
            if (leading_period(s, str, flags)) goto fail;
            r = bracket(&q, c, flags);
            if (r < 0) { if (c != '[') goto fail; p++; s++; continue; }
            if (!r) goto fail;
            p = q; s++;
            continue;
        }
        {
            int pc = (unsigned char)*p;
            if (pc == '\\' && !(flags & FNM_NOESCAPE) && p[1]) pc = (unsigned char)*++p;
            if (fold(pc, flags) != fold(c, flags)) goto fail;
            p++; s++;
            continue;
        }
    fail:
        if (!star_p) return FNM_NOMATCH;
        if ((flags & FNM_PATHNAME) && *star_s == '/') return FNM_NOMATCH;   /* '*' cannot cross '/' */
        if (!*star_s) return FNM_NOMATCH;
        p = star_p;
        s = ++star_s;
    }
}
