#include <locale.h>
#include <string.h>
#include <limits.h>

char *setlocale(int category, const char *locale)
{
    if (category < 0 || category > LC_ALL) return NULL;
    if (!locale || !*locale || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0)
        return (char *)"C";
    return NULL;
}

struct lconv *localeconv(void)
{
    static struct lconv c = {
        ".", "", "", "", "", "", "", "", "", "",
        CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
        CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX
    };
    return &c;
}
