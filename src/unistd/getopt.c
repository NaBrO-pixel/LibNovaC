/* getopt, getopt_long, getopt_long_only.
 *
 * POSIX getopt plus the GNU long-option extensions. By default argv is
 * permuted as GNU does: each call skips over non-options, handles one
 * option, then rotates the words it consumed in front of the skipped ones,
 * so that when -1 comes back argv[optind..] holds the operands in order.
 * An optstring starting with '+' (or POSIXLY_CORRECT in the environment)
 * stops at the first operand; one starting with '-' returns each operand
 * as the argument of option 1. "--" ends the options. Setting optind to 0
 * restarts. Messages go to stderr unless opterr is 0 or the optstring
 * (after any '+'/'-') starts with ':'. */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../internal/sys.h"

char *optarg;
int optind = 1, opterr = 1, optopt;
static int pos;                       /* index inside a cluster like "-abc"; 0 = at its start */

static void complain(int quiet, const char *prog, const char *fmt, const char *s, int c)
{
    char buf[256];
    int n;
    if (!opterr || quiet) return;
    n = s ? snprintf(buf, sizeof buf, fmt, prog, s) : snprintf(buf, sizeof buf, fmt, prog, c);
    if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;
    if (n > 0) novac_sys_write_stderr(buf, (size_t)n);
}

static int long_opt(int argc, char **argv, const char *opts, const struct option *lo,
                    int *idx, int quiet, int dashes)
{
    const char *name = argv[optind] + dashes;
    size_t nlen = strcspn(name, "=");
    int i, found = -1, ambiguous = 0;
    const struct option *o;

    for (i = 0; lo[i].name; i++) {
        if (strncmp(lo[i].name, name, nlen) != 0) continue;
        if (strlen(lo[i].name) == nlen) { found = i; ambiguous = 0; break; }   /* exact */
        if (found < 0) found = i;
        else if (lo[i].has_arg != lo[found].has_arg || lo[i].flag != lo[found].flag ||
                 lo[i].val != lo[found].val) ambiguous = 1;
    }
    if (found < 0 || ambiguous) {
        if (dashes == 1 && strchr(opts, name[0])) return -2;   /* long_only: try it as short */
        complain(quiet, argv[0], ambiguous ? "%s: option '%s' is ambiguous\n"
                                           : "%s: unrecognized option '%s'\n", argv[optind], 0);
        optind++;
        optopt = 0;
        return '?';
    }
    o = &lo[found];
    optind++;
    if (name[nlen] == '=') {
        if (o->has_arg == no_argument) {
            complain(quiet, argv[0], "%s: option '--%s' doesn't allow an argument\n", o->name, 0);
            optopt = o->val;
            return '?';
        }
        optarg = (char *)name + nlen + 1;
    } else if (o->has_arg == required_argument) {
        if (optind >= argc) {
            complain(quiet, argv[0], "%s: option '--%s' requires an argument\n", o->name, 0);
            optopt = o->val;
            return opts[0] == ':' ? ':' : '?';
        }
        optarg = argv[optind++];
    }
    if (idx) *idx = found;
    if (o->flag) { *o->flag = o->val; return 0; }
    return o->val;
}

static int core(int argc, char **argv, const char *opts, const struct option *lo,
                int *idx, int long_only)
{
    const char *arg;
    int c, quiet;
    const char *spec;

    if (*opts == '+' || *opts == '-') opts++;
    quiet = opts[0] == ':';
    optarg = NULL;
    if (optind >= argc || !argv[optind]) return -1;
    arg = argv[optind];
    if (pos == 0) {
        if (arg[0] != '-' || !arg[1]) return -1;
        if (arg[1] == '-' && !arg[2]) { optind++; return -1; }   /* "--" */
        if (lo && arg[1] == '-') return long_opt(argc, argv, opts, lo, idx, quiet, 2);
        if (lo && long_only) {
            int r = long_opt(argc, argv, opts, lo, idx, quiet, 1);
            if (r != -2) return r;
        }
        pos = 1;
    }
    c = (unsigned char)arg[pos++];
    spec = (c == ':') ? NULL : strchr(opts, c);
    if (!spec) {
        optopt = c;
        complain(quiet, argv[0], "%s: invalid option -- '%c'\n", NULL, c);
        if (!arg[pos]) { optind++; pos = 0; }
        return '?';
    }
    if (spec[1] != ':') {                         /* no argument */
        if (!arg[pos]) { optind++; pos = 0; }
        return c;
    }
    if (arg[pos]) optarg = (char *)arg + pos;     /* "-ovalue" */
    else if (spec[2] != ':') {                     /* required: the next word */
        if (optind + 1 >= argc) {
            optopt = c;
            complain(quiet, argv[0], "%s: option requires an argument -- '%c'\n", NULL, c);
            optind++; pos = 0;
            return quiet ? ':' : '?';
        }
        optarg = argv[++optind];
    }
    optind++; pos = 0;
    return c;
}

/* argv[src] moves to argv[dest] (dest < src), the words between shift up */
static void rotate(char **argv, int dest, int src)
{
    char *t = argv[src];
    memmove(&argv[dest + 1], &argv[dest], (size_t)(src - dest) * sizeof *argv);
    argv[dest] = t;
}

static int driver(int argc, char *const argv_[], const char *opts, const struct option *lo,
                  int *idx, int long_only)
{
    char **argv = (char **)argv_;
    int skipped, resumed, ret;
    int permute = opts[0] != '+' && !getenv("POSIXLY_CORRECT");

    if (optind == 0) { optind = 1; pos = 0; }
    if (optind >= argc || !argv[optind]) return -1;
    if (opts[0] == '-' && pos == 0 && (argv[optind][0] != '-' || !argv[optind][1])) {
        optarg = argv[optind++];                  /* in-order mode: operands as option 1 */
        return 1;
    }
    skipped = optind;
    if (permute && opts[0] != '-') {              /* also mid-cluster: operands may precede it */
        int i = optind;
        while (i < argc && argv[i] && !(argv[i][0] == '-' && argv[i][1])) i++;
        if (i >= argc || !argv[i]) return -1;      /* only operands left: optind stays on them */
        optind = i;
    }
    resumed = optind;
    ret = core(argc, argv, opts, lo, idx, long_only);
    if (resumed > skipped) {
        /* move the words fully consumed in front of the operands skipped. A
         * cluster still being read stays put (optind then points back at the
         * operands), so its argument, if any, is still the word after it. */
        int i, cnt = optind - resumed;
        for (i = 0; i < cnt; i++) rotate(argv, skipped + i, resumed + i);
        optind = skipped + cnt;
    }
    return ret;
}

int getopt(int argc, char *const argv[], const char *opts)
{
    return driver(argc, argv, opts, NULL, NULL, 0);
}
int getopt_long(int argc, char *const argv[], const char *opts, const struct option *l, int *idx)
{
    return driver(argc, argv, opts, l, idx, 0);
}
int getopt_long_only(int argc, char *const argv[], const char *opts, const struct option *l, int *idx)
{
    return driver(argc, argv, opts, l, idx, 1);
}
