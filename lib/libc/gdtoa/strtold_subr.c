/* $NetBSD: strtold_subr.c,v 1.3 2013/05/17 12:55:57 joerg Exp $ */

/*
 * Written by Klaus Klein <kleink@NetBSD.org>, November 16, 2005.
 * Public domain.
 */

/*
 * NOTICE: This is not a standalone file.  To use it, #include it in
 * the format-specific strtold_*.c, like so:
 *
 *	#define GDTOA_LD_FMT	<gdtoa extended-precision format code>
 *	#include "strtold_subr.c"
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: strtold_subr.c,v 1.3 2013/05/17 12:55:57 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <math.h>
#include <stdlib.h>
#include "gdtoa.h"

#include <locale.h>
#include "setlocale_local.h"

#ifdef __weak_alias
__weak_alias(strtold, _strtold)
__weak_alias(strtold_l, _strtold_l)
#endif

#ifndef __HAVE_LONG_DOUBLE
#error no extended-precision long double type
#endif

#ifndef GDTOA_LD_FMT
#error GDTOA_LD_FMT must be defined by format-specific source file
#endif

#define	STRTOP(x)	__CONCAT(strtop, x)

static long double
_int_strtold_l(const char *nptr, char **endptr, locale_t loc)
{
	long double ld;

	(void)STRTOP(GDTOA_LD_FMT)(nptr, endptr, &ld, loc);
	return ld;
}

long double
strtold(CONST char *s, char **sp)
{
	return _int_strtold_l(s, sp, _current_locale());
}

long double
strtold_l(CONST char *s, char **sp, locale_t loc)
{
	return _int_strtold_l(s, sp, loc);
}
