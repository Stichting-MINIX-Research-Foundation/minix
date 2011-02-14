/*	$NetBSD: _vwarnx.c,v 1.10 2005/09/13 01:44:09 christos Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _vwarnx.c,v 1.10 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_vwarnx, vwarnx)
#else

void _vwarnx(const char *, _BSD_VA_LIST_);

void
vwarnx(const char *fmt, _BSD_VA_LIST_ ap)
{
	_vwarnx(fmt, ap);
}

#endif
