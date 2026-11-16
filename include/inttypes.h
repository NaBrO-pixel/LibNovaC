/* <inttypes.h> for x86-64 (LP64). */
#ifndef NOVAC_INTTYPES_H
#define NOVAC_INTTYPES_H
#include <stdint.h>
#include <novac/defs.h>

typedef struct { intmax_t quot, rem; } imaxdiv_t;

#define NOVAC_PRI64 "l"
#define PRId8  "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 NOVAC_PRI64 "d"
#define PRIi8  "i"
#define PRIi16 "i"
#define PRIi32 "i"
#define PRIi64 NOVAC_PRI64 "i"
#define PRIu8  "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 NOVAC_PRI64 "u"
#define PRIo8  "o"
#define PRIo16 "o"
#define PRIo32 "o"
#define PRIo64 NOVAC_PRI64 "o"
#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 NOVAC_PRI64 "x"
#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 NOVAC_PRI64 "X"
#define PRIdLEAST8  PRId8
#define PRIdLEAST16 PRId16
#define PRIdLEAST32 PRId32
#define PRIdLEAST64 PRId64
#define PRIuLEAST8  PRIu8
#define PRIuLEAST16 PRIu16
#define PRIuLEAST32 PRIu32
#define PRIuLEAST64 PRIu64
#define PRIxLEAST64 PRIx64
#define PRIdFAST8  "d"
#define PRIdFAST16 NOVAC_PRI64 "d"
#define PRIdFAST32 NOVAC_PRI64 "d"
#define PRIdFAST64 PRId64
#define PRIuFAST8  "u"
#define PRIuFAST16 NOVAC_PRI64 "u"
#define PRIuFAST32 NOVAC_PRI64 "u"
#define PRIuFAST64 PRIu64
#define PRIdMAX NOVAC_PRI64 "d"
#define PRIiMAX NOVAC_PRI64 "i"
#define PRIuMAX NOVAC_PRI64 "u"
#define PRIxMAX NOVAC_PRI64 "x"
#define PRIXMAX NOVAC_PRI64 "X"
#define PRIoMAX NOVAC_PRI64 "o"
#define PRIdPTR NOVAC_PRI64 "d"
#define PRIiPTR NOVAC_PRI64 "i"
#define PRIuPTR NOVAC_PRI64 "u"
#define PRIxPTR NOVAC_PRI64 "x"
#define PRIXPTR NOVAC_PRI64 "X"
#define PRIoPTR NOVAC_PRI64 "o"

#define SCNd8  "hhd"
#define SCNd16 "hd"
#define SCNd32 "d"
#define SCNd64 NOVAC_PRI64 "d"
#define SCNi32 "i"
#define SCNi64 NOVAC_PRI64 "i"
#define SCNu8  "hhu"
#define SCNu16 "hu"
#define SCNu32 "u"
#define SCNu64 NOVAC_PRI64 "u"
#define SCNx8  "hhx"
#define SCNx16 "hx"
#define SCNx32 "x"
#define SCNx64 NOVAC_PRI64 "x"
#define SCNo32 "o"
#define SCNo64 NOVAC_PRI64 "o"
#define SCNdMAX NOVAC_PRI64 "d"
#define SCNuMAX NOVAC_PRI64 "u"
#define SCNxMAX NOVAC_PRI64 "x"
#define SCNdPTR NOVAC_PRI64 "d"
#define SCNuPTR NOVAC_PRI64 "u"
#define SCNxPTR NOVAC_PRI64 "x"

NOVAC_BEGIN_DECLS
intmax_t  imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t n, intmax_t d);
intmax_t  strtoimax(const char *restrict s, char **restrict end, int base);
uintmax_t strtoumax(const char *restrict s, char **restrict end, int base);
NOVAC_END_DECLS
#endif
