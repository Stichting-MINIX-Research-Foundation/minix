/*	$NetBSD: _catgets.c,v 1.8 2005/09/13 01:44:10 christos Exp $	*/

/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _catgets.c,v 1.8 2005/09/13 01:44:10 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_catgets, catgets)
#else

#include <nl_types.h>
char	*_catgets(nl_catd, int, int, const char *);

char *
catgets(nl_catd catd, int set_id, int msg_id, const char *s)
{
	return _catgets(catd, set_id, msg_id, s);
}

#endif
