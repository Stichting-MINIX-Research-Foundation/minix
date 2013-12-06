/*	$NetBSD: _warn.c,v 1.11 2011/07/17 20:54:34 joerg Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _warn.c,v 1.11 2011/07/17 20:54:34 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_warn, warn)
#else

#include <stdarg.h>

void _vwarn(const char *, va_list);

void
warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_vwarn(eval, fmt, ap);
	va_end(ap);
}
#endif
