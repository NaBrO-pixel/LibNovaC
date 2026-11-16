/* Shared by the target test's source files. */
#ifndef NOVAC_TARGET_H
#define NOVAC_TARGET_H
/* Prints "ok   what" or "FAIL what" and counts failures. Every line must
 * read the same on Linux and on NovarisOS. */
void ok(const char *what, int cond);
void test_v03(void);
#endif
