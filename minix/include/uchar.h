/*	uchar.h - C11 Unicode character types for MINIX.
 *
 * char16_t and char32_t, and the char32_t conversions mbrtoc32() and
 * c32rtomb().  These map onto mbrtowc()/wcrtomb(): MINIX's wchar_t holds
 * Unicode code points in the C and UTF-8 locales (the only ones whose
 * wchar_t encoding is Unicode; other locales give other values).  The
 * char16_t conversions (mbrtoc16, c16rtomb) are not provided: they need
 * surrogate state in mbstate_t, which belongs to the locale code.
 *
 * $MINIX$
 */

#ifndef _UCHAR_H_
#define _UCHAR_H_

#include <sys/cdefs.h>
#include <sys/ansi.h>
#include <stdint.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_MBSTATE_T_
typedef	_BSD_MBSTATE_T_	mbstate_t;
#undef	_BSD_MBSTATE_T_
#endif

#if !defined(__cplusplus) || __cplusplus < 201103L
typedef	uint_least16_t	char16_t;
typedef	uint_least32_t	char32_t;
#endif

__BEGIN_DECLS
size_t	mbrtoc32(char32_t * __restrict, const char * __restrict, size_t,
	    mbstate_t * __restrict);
size_t	c32rtomb(char * __restrict, char32_t, mbstate_t * __restrict);
__END_DECLS

#endif /* _UCHAR_H_ */
