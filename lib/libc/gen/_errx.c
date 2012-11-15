/*	$NetBSD: _errx.c,v 1.12 2011/07/17 20:54:34 joerg Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _errx.c,v 1.12 2011/07/17 20:54:34 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_errx, errx)
#else

#include <stdarg.h>

__dead void _verrx(int eval, const char *, va_list);

__dead void
errx(int eval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_verrx(eval, fmt, ap);
	va_end(ap);
}
#endif
