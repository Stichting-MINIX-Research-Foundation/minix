/*	$NetBSD: _catopen.c,v 1.7 2005/09/13 01:44:10 christos Exp $	*/

/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _catopen.c,v 1.7 2005/09/13 01:44:10 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_catopen, catopen)
#else

#include <nl_types.h>
nl_catd	_catopen(__const char *, int);

nl_catd
catopen(__const char *name, int oflag)
{
	return _catopen(name, oflag);
}

#endif
