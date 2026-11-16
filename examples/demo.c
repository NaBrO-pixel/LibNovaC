/* A small libNovaC program: word statistics for its arguments (or a
 * built-in sentence), using qsort, strtok_r, a memory stream, floating-point
 * printf and <math.h> - with no host C library at all.
 *
 *   make example && ./build/target/examples/demo the quick brown fox
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int by_length(const void *a, const void *b)
{
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    size_t lx = strlen(x), ly = strlen(y);
    return lx != ly ? (lx < ly ? -1 : 1) : strcmp(x, y);
}

int main(int argc, char **argv)
{
    char text[] = "libNovaC brings its own start-up code, stdio, math and allocator";
    char *words[64], *save, *w, *report = NULL;
    size_t n = 0, i, size = 0;
    double mean = 0, var = 0;
    FILE *out;

    if (argc > 1) for (i = 1; i < (size_t)argc && n < 64; i++) words[n++] = argv[i];
    else for (w = strtok_r(text, " ,", &save); w && n < 64; w = strtok_r(NULL, " ,", &save)) words[n++] = w;
    qsort(words, n, sizeof words[0], by_length);

    for (i = 0; i < n; i++) mean += (double)strlen(words[i]);
    mean /= (double)(n ? n : 1);
    for (i = 0; i < n; i++) var += pow((double)strlen(words[i]) - mean, 2);

    out = open_memstream(&report, &size);
    if (!out) return 1;
    fprintf(out, "%zu words, shortest \"%s\", longest \"%s\"\n", n, n ? words[0] : "", n ? words[n - 1] : "");
    fprintf(out, "mean length %.3f, standard deviation %.3f\n", mean, sqrt(var / (double)(n ? n : 1)));
    fclose(out);
    fputs(report, stdout);
    free(report);
    return 0;
}
