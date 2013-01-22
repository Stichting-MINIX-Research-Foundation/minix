/*	$NetBSD: multibyte.h,v 1.7 2011/11/23 15:43:39 tnozaki Exp $ */

#ifndef MULTIBYTE_H
#define MULTIBYTE_H

/*
 * Ex/vi commands are generally separated by whitespace characters.  We
 * can't use the standard isspace(3) macro because it returns true for
 * characters like ^K in the ASCII character set.  The 4.4BSD isblank(3)
 * macro does exactly what we want, but it's not portable yet.
 *
 * XXX
 * Note side effect, ch is evaluated multiple times.
 */
#define ISBLANK(c)	((c) == ' ' || (c) == '\t')

#define ISDIGIT(c)	((c) >= '0' && (c) <= '9')
#define ISXDIGIT(c)	(ISDIGIT(c) || \
			 ((c) >= 'A' && (c) <= 'F') || ((c) >= 'a' && (c) <= 'f'))
#define ISALPHA(c)	(((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define ISALNUM(c)	(ISALPHA(c) || ISDIGIT(c))

/*
 * Fundamental character types.
 *
 * CHAR_T       An integral type that can hold any character.
 * ARG_CHAR_T   The type of a CHAR_T when passed as an argument using
 *              traditional promotion rules.  It should also be able
 *              to be compared against any CHAR_T for equality without
 *              problems.
 *
 * If no integral type can hold a character, don't even try the port.
 */

#ifdef USE_WIDECHAR
#include <wchar.h>
#include <wctype.h>

typedef wchar_t		RCHAR_T;
#define REOF		WEOF
typedef wchar_t		CHAR_T;
typedef	wint_t		ARG_CHAR_T;
typedef wint_t		UCHAR_T;

#define STRLEN		wcslen
#define STRTOL		wcstol
#define STRTOUL		wcstoul
#define SPRINTF		swprintf
#define STRCMP		wcscmp
#define STRPBRK		wcspbrk
#define ISBLANK2	iswblank
#define ISCNTRL		iswcntrl
#define ISGRAPH		iswgraph
#define ISLOWER		iswlower
#define ISPUNCT		iswpunct
#define ISSPACE		iswspace
#define ISUPPER		iswupper
#define TOLOWER		towlower
#define TOUPPER		towupper
#define STRSET		wmemset
#define STRCHR		wcschr

#define L(ch)		L ## ch
#define WS		"%ls"
#define WVS		"%*ls"
#define WC		"%lc"

#else
#include <stdio.h>

typedef	char		RCHAR_T;
#define REOF		EOF
typedef	char		CHAR_T;
typedef	int		ARG_CHAR_T;
typedef	unsigned char	UCHAR_T;

#define STRLEN		strlen
#define STRTOL		strtol
#define STRTOUL		strtoul
#define SPRINTF		snprintf
#define STRCMP		strcmp
#define STRPBRK		strpbrk
#define ISBLANK2	isblank
#define ISCNTRL		iscntrl
#define ISGRAPH		isgraph
#define ISLOWER		islower
#define ISPUNCT		ispunct
#define ISSPACE		isspace
#define ISUPPER		isupper
#define TOLOWER		tolower
#define TOUPPER		toupper
#define STRSET		memset
#define STRCHR		strchr

#define L(ch)		ch
#define WS		"%s"
#define WVS		"%*s"
#define WC		"%c"

#endif

#define MEMCMP(to, from, n) 						    \
	memcmp(to, from, (n) * sizeof(*(to)))
#define	MEMMOVE(p, t, len)	memmove(p, t, (len) * sizeof(*(p)))
#define	MEMCPY(p, t, len)	memcpy(p, t, (len) * sizeof(*(p)))
#define SIZE(w)		(sizeof(w)/sizeof(*w))

#endif
