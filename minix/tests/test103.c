/*
 * test103: C11 <uchar.h> char32_t conversions, mbrtoc32() and c32rtomb(),
 * in a UTF-8 locale (as foot uses them): 1- to 4-byte sequences both ways,
 * incomplete input (-2) completed by a second call, invalid bytes,
 * surrogates and values beyond U+10FFFF, and the NULL-buffer forms.
 */
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

int max_error = 5;
#include "common.h"

static const struct {
	char32_t	c;
	const char	*utf8;
} cases[] = {
	{ 0x41, "A" },
	{ 0xe9, "\xc3\xa9" },			/* e acute */
	{ 0x20ac, "\xe2\x82\xac" },		/* euro sign */
	{ 0x1f600, "\xf0\x9f\x98\x80" },	/* emoji (outside the BMP) */
};

static void
test_utf8(void)
{
	char buf[MB_LEN_MAX];
	mbstate_t st;
	char32_t c;
	size_t i, len, r;

	subtest = 1;

	if (setlocale(LC_CTYPE, "en_US.UTF-8") == NULL) e(1);

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		len = strlen(cases[i].utf8);

		memset(&st, 0, sizeof(st));
		c = 0;
		r = mbrtoc32(&c, cases[i].utf8, len, &st);
		if (r != len || c != cases[i].c) e(2);

		memset(&st, 0, sizeof(st));
		r = c32rtomb(buf, cases[i].c, &st);
		if (r != len || memcmp(buf, cases[i].utf8, len) != 0) e(3);
	}

	/* An incomplete sequence, completed by the next call. */
	memset(&st, 0, sizeof(st));
	if (mbrtoc32(&c, "\xf0\x9f", 2, &st) != (size_t)-2) e(4);
	c = 0;
	if (mbrtoc32(&c, "\x98\x80", 2, &st) != 2 || c != 0x1f600) e(5);

	/* The terminating NUL converts to 0 and returns 0. */
	memset(&st, 0, sizeof(st));
	c = 1;
	if (mbrtoc32(&c, "", 1, &st) != 0 || c != 0) e(6);

	/* Invalid input, surrogates and out-of-range values. */
	memset(&st, 0, sizeof(st));
	errno = 0;
	if (mbrtoc32(&c, "\xff", 1, &st) != (size_t)-1 || errno != EILSEQ)
		e(7);
	memset(&st, 0, sizeof(st));
	errno = 0;
	if (c32rtomb(buf, 0xd800, &st) != (size_t)-1 || errno != EILSEQ) e(8);
	errno = 0;
	if (c32rtomb(buf, 0x110000, &st) != (size_t)-1 || errno != EILSEQ)
		e(9);

	/* NULL buffers: reset the state, the size of a NUL. */
	if (c32rtomb(NULL, 0x20ac, NULL) != 1) e(10);
	if (mbrtoc32(NULL, NULL, 0, NULL) != 0) e(11);
	/* No output pointer: still the length. */
	memset(&st, 0, sizeof(st));
	if (mbrtoc32(NULL, "\xe2\x82\xac", 3, &st) != 3) e(12);
}

static void
test_c_locale(void)
{
	char buf[MB_LEN_MAX];
	char32_t c;

	subtest = 2;

	if (setlocale(LC_CTYPE, "C") == NULL) e(1);
	if (mbrtoc32(&c, "z", 1, NULL) != 1 || c != 'z') e(2);
	if (c32rtomb(buf, 'q', NULL) != 1 || buf[0] != 'q') e(3);
}

int
main(int argc, char **argv)
{

	start(103);

	test_utf8();
	test_c_locale();

	quit();
	return 0;
}
