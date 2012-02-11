/*	$NetBSD: _catclose.c,v 1.7 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _catclose.c,v 1.7 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_catclose, catclose)
#else

#include <nl_types.h>
int	_catclose(nl_catd);

int
catclose(nl_catd catd)
{
	return _catclose(catd);
}

#endif
