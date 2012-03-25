#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_64_BIT

#define ERR e(__LINE__)

#define MAX_ERROR 4
#include "common.c"

#define TEST_PRINTF(type, macro, value, result)				\
{									\
	char buffer[256];						\
	snprintf(buffer, sizeof(buffer), "%" macro, (type) value);	\
	if (strcmp(buffer, result) != 0) ERR;			\
}

int main(void)
{
	start(49);

	/* test integer sizes */
	if (sizeof(int8_t) != 1) ERR;
	if (sizeof(uint8_t) != 1) ERR;
	if (sizeof(int_fast8_t) < 1) ERR;
	if (sizeof(uint_fast8_t) < 1) ERR;
	if (sizeof(int_least8_t) < 1) ERR;
	if (sizeof(uint_least8_t) < 1) ERR;
	if (sizeof(int16_t) != 2) ERR;
	if (sizeof(uint16_t) != 2) ERR;
	if (sizeof(int_fast16_t) < 2) ERR;
	if (sizeof(uint_fast16_t) < 2) ERR;
	if (sizeof(int_least16_t) < 2) ERR;
	if (sizeof(uint_least16_t) < 2) ERR;
	if (sizeof(int32_t) != 4) ERR;
	if (sizeof(uint32_t) != 4) ERR;
	if (sizeof(int_fast32_t) < 4) ERR;
	if (sizeof(uint_fast32_t) < 4) ERR;
	if (sizeof(int_least32_t) < 4) ERR;
	if (sizeof(uint_least32_t) < 4) ERR;
#ifdef TEST_64_BIT
	if (sizeof(int64_t) != 8) ERR;
	if (sizeof(uint64_t) != 8) ERR;
	if (sizeof(int_fast64_t) < 8) ERR;
	if (sizeof(uint_fast64_t) < 8) ERR;
	if (sizeof(int_least64_t) < 8) ERR;
	if (sizeof(uint_least64_t) < 8) ERR;
#endif
	if (sizeof(intptr_t) != sizeof(void *)) ERR;
	if (sizeof(uintptr_t) != sizeof(void *)) ERR;
#ifdef TEST_64_BIT
	if (sizeof(intmax_t) < 8) ERR;
	if (sizeof(uintmax_t) < 8) ERR;
#else
	if (sizeof(intmax_t) < 4) ERR;
	if (sizeof(uintmax_t) < 4) ERR;
#endif

	/* test integer signedness */
	if ((int8_t) (-1) >= 0) ERR;
	if ((uint8_t) (-1) <= 0) ERR;
	if ((int_fast8_t) (-1) >= 0) ERR;
	if ((uint_fast8_t) (-1) <= 0) ERR;
	if ((int_least8_t) (-1) >= 0) ERR;
	if ((uint_least8_t) (-1) <= 0) ERR;
	if ((int16_t) (-1) >= 0) ERR;
	if ((uint16_t) (-1) <= 0) ERR;
	if ((int_fast16_t) (-1) >= 0) ERR;
	if ((uint_fast16_t) (-1) <= 0) ERR;
	if ((int_least16_t) (-1) >= 0) ERR;
	if ((uint_least16_t) (-1) <= 0) ERR;
	if ((int32_t) (-1) >= 0) ERR;
	if ((uint32_t) (-1) <= 0) ERR;
	if ((int_fast32_t) (-1) >= 0) ERR;
	if ((uint_fast32_t) (-1) <= 0) ERR;
	if ((int_least32_t) (-1) >= 0) ERR;
	if ((uint_least32_t) (-1) <= 0) ERR;
#ifdef TEST_64_BIT
	if ((int64_t) (-1) >= 0) ERR;
	if ((uint64_t) (-1) <= 0) ERR;
	if ((int_fast64_t) (-1) >= 0) ERR;
	if ((uint_fast64_t) (-1) <= 0) ERR;
	if ((int_least64_t) (-1) >= 0) ERR;
	if ((uint_least64_t) (-1) <= 0) ERR;
#endif
	if ((intptr_t) (-1) >= 0) ERR;
	if ((uintptr_t) (-1) <= 0) ERR;
	if ((intptr_t) (-1) >= 0) ERR;
	if ((uintptr_t) (-1) <= 0) ERR;

	/* test printf */
	TEST_PRINTF(int32_t,  PRId32, INT32_MIN,  "-2147483648");
	TEST_PRINTF(int32_t,  PRId32, INT32_MAX,   "2147483647");
	TEST_PRINTF(int32_t,  PRIi32, INT32_MIN,  "-2147483648");
	TEST_PRINTF(int32_t,  PRIi32, INT32_MAX,   "2147483647");
	TEST_PRINTF(uint32_t, PRIu32, UINT32_MAX,  "4294967295");
	TEST_PRINTF(uint32_t, PRIX32, UINT32_MAX,    "FFFFFFFF");
	TEST_PRINTF(uint32_t, PRIx32, UINT32_MAX,    "ffffffff");
	TEST_PRINTF(uint32_t, PRIo32, UINT32_MAX, "37777777777");

	TEST_PRINTF(int_fast32_t,  PRIdFAST32, INT32_MIN,  "-2147483648");
	TEST_PRINTF(int_fast32_t,  PRIdFAST32, INT32_MAX,   "2147483647");
	TEST_PRINTF(int_fast32_t,  PRIiFAST32, INT32_MIN,  "-2147483648");
	TEST_PRINTF(int_fast32_t,  PRIiFAST32, INT32_MAX,   "2147483647");
	TEST_PRINTF(uint_fast32_t, PRIuFAST32, UINT32_MAX,  "4294967295");
	TEST_PRINTF(uint_fast32_t, PRIXFAST32, UINT32_MAX,    "FFFFFFFF");
	TEST_PRINTF(uint_fast32_t, PRIxFAST32, UINT32_MAX,    "ffffffff");
	TEST_PRINTF(uint_fast32_t, PRIoFAST32, UINT32_MAX, "37777777777");

	TEST_PRINTF(int_least32_t,  PRIdLEAST32, INT32_MIN,  "-2147483648");
	TEST_PRINTF(int_least32_t,  PRIdLEAST32, INT32_MAX,   "2147483647");
	TEST_PRINTF(int_least32_t,  PRIiLEAST32, INT32_MIN,  "-2147483648");
	TEST_PRINTF(int_least32_t,  PRIiLEAST32, INT32_MAX,   "2147483647");
	TEST_PRINTF(uint_least32_t, PRIuLEAST32, UINT32_MAX,  "4294967295");
	TEST_PRINTF(uint_least32_t, PRIXLEAST32, UINT32_MAX,    "FFFFFFFF");
	TEST_PRINTF(uint_least32_t, PRIxLEAST32, UINT32_MAX,    "ffffffff");
	TEST_PRINTF(uint_least32_t, PRIoLEAST32, UINT32_MAX, "37777777777");

#ifdef TEST_64_BIT
	TEST_PRINTF(int64_t,  PRId64, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(int64_t,  PRId64, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(int64_t,  PRIi64, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(int64_t,  PRIi64, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(uint64_t, PRIu64, UINT64_MAX,   "18446744073709551615");
	TEST_PRINTF(uint64_t, PRIX64, UINT64_MAX,       "FFFFFFFFFFFFFFFF");
	TEST_PRINTF(uint64_t, PRIx64, UINT64_MAX,       "ffffffffffffffff");
	TEST_PRINTF(uint64_t, PRIo64, UINT64_MAX, "1777777777777777777777");

	TEST_PRINTF(int_fast64_t,  PRIdFAST64, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(int_fast64_t,  PRIdFAST64, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(int_fast64_t,  PRIiFAST64, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(int_fast64_t,  PRIiFAST64, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(uint_fast64_t, PRIuFAST64, UINT64_MAX,   "18446744073709551615");
	TEST_PRINTF(uint_fast64_t, PRIXFAST64, UINT64_MAX,       "FFFFFFFFFFFFFFFF");
	TEST_PRINTF(uint_fast64_t, PRIxFAST64, UINT64_MAX,       "ffffffffffffffff");
	TEST_PRINTF(uint_fast64_t, PRIoFAST64, UINT64_MAX, "1777777777777777777777");

	TEST_PRINTF(int_least64_t,  PRIdLEAST64, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(int_least64_t,  PRIdLEAST64, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(int_least64_t,  PRIiLEAST64, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(int_least64_t,  PRIiLEAST64, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(uint_least64_t, PRIuLEAST64, UINT64_MAX,   "18446744073709551615");
	TEST_PRINTF(uint_least64_t, PRIXLEAST64, UINT64_MAX,       "FFFFFFFFFFFFFFFF");
	TEST_PRINTF(uint_least64_t, PRIxLEAST64, UINT64_MAX,       "ffffffffffffffff");
	TEST_PRINTF(uint_least64_t, PRIoLEAST64, UINT64_MAX, "1777777777777777777777");

	TEST_PRINTF(intmax_t,  PRIdMAX, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(intmax_t,  PRIdMAX, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(intmax_t,  PRIiMAX, INT64_MIN,    "-9223372036854775808");
	TEST_PRINTF(intmax_t,  PRIiMAX, INT64_MAX,     "9223372036854775807");
	TEST_PRINTF(uintmax_t, PRIuMAX, UINT64_MAX,   "18446744073709551615");
	TEST_PRINTF(uintmax_t, PRIXMAX, UINT64_MAX,       "FFFFFFFFFFFFFFFF");
	TEST_PRINTF(uintmax_t, PRIxMAX, UINT64_MAX,       "ffffffffffffffff");
	TEST_PRINTF(uintmax_t, PRIoMAX, UINT64_MAX, "1777777777777777777777");
#else
	TEST_PRINTF(intmax_t,  PRIdMAX, INT32_MIN,  "-2147483648");
	TEST_PRINTF(intmax_t,  PRIdMAX, INT32_MAX,   "2147483647");
	TEST_PRINTF(intmax_t,  PRIiMAX, INT32_MIN,  "-2147483648");
	TEST_PRINTF(intmax_t,  PRIiMAX, INT32_MAX,   "2147483647");
	TEST_PRINTF(uintmax_t, PRIuMAX, UINT32_MAX,  "4294967295");
	TEST_PRINTF(uintmax_t, PRIXMAX, UINT32_MAX,    "FFFFFFFF");
	TEST_PRINTF(uintmax_t, PRIxMAX, UINT32_MAX,    "ffffffff");
	TEST_PRINTF(uintmax_t, PRIoMAX, UINT32_MAX, "37777777777");
#endif

	TEST_PRINTF(intptr_t,  PRIdPTR, INT32_MIN,  "-2147483648");
	TEST_PRINTF(intptr_t,  PRIdPTR, INT32_MAX,   "2147483647");
	TEST_PRINTF(intptr_t,  PRIiPTR, INT32_MIN,  "-2147483648");
	TEST_PRINTF(intptr_t,  PRIiPTR, INT32_MAX,   "2147483647");
	TEST_PRINTF(uintptr_t, PRIuPTR, UINT32_MAX,  "4294967295");
	TEST_PRINTF(uintptr_t, PRIXPTR, UINT32_MAX,    "FFFFFFFF");
	TEST_PRINTF(uintptr_t, PRIxPTR, UINT32_MAX,    "ffffffff");
	TEST_PRINTF(uintptr_t, PRIoPTR, UINT32_MAX, "37777777777");

	/* done */
	quit();
	return -1;
}
