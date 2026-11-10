/* uchar.c - C11 char32_t conversions (<uchar.h>) for MINIX.
 *
 * MINIX's wchar_t is 32 bits wide and holds Unicode code points in the C
 * and UTF-8 locales, so mbrtoc32() and c32rtomb() are mbrtowc() and
 * wcrtomb() with the other type.  Values above the Unicode range are not
 * characters.
 */

#include <sys/cdefs.h>

#include <errno.h>
#include <uchar.h>
#include <wchar.h>

size_t
mbrtoc32(char32_t * __restrict pc32, const char * __restrict s, size_t n,
    mbstate_t * __restrict ps)
{
	static mbstate_t state;
	wchar_t wc;
	size_t r;

	if (ps == NULL)
		ps = &state;
	if (s == NULL)
		return mbrtowc(NULL, "", 1, ps);
	r = mbrtowc(&wc, s, n, ps);
	if (r <= n && pc32 != NULL)
		*pc32 = (char32_t)wc;
	return r;
}

size_t
c32rtomb(char * __restrict s, char32_t c32, mbstate_t * __restrict ps)
{
	static mbstate_t state;

	if (ps == NULL)
		ps = &state;
	if (s == NULL)
		return wcrtomb(NULL, L'\0', ps);
	if (c32 > 0x10ffff || (c32 >= 0xd800 && c32 <= 0xdfff)) {
		errno = EILSEQ;
		return (size_t)-1;
	}
	return wcrtomb(s, (wchar_t)c32, ps);
}
