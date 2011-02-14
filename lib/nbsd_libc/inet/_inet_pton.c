/*	$NetBSD: _inet_pton.c,v 1.4 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Written by Klaus Klein, September 14, 1999.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _inet_pton.c,v 1.4 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_inet_pton, inet_pton)
#else

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int	_inet_pton(int, const char *, void *);

int
inet_pton(int af, const char *src, void *dst)
{

	return _inet_pton(af, src, dst);
}
#endif
