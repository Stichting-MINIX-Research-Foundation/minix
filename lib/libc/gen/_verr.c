/*	$NetBSD: _verr.c,v 1.11 2011/07/17 20:54:34 joerg Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _verr.c,v 1.11 2011/07/17 20:54:34 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include <stdarg.h>

#if defined(__indr_reference)
__indr_reference(_verr, verr)
#else

__dead void _verr(int, const char *, va_list);

void
verr(int eval, const char *fmt, va_list ap)
{
	_verr(eval, fmt, ap);
}

#endif
