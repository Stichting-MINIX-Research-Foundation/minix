/*	$NetBSD: _inet_aton.c,v 1.4 2005/09/13 01:44:09 christos Exp $	*/

/*
 * Written by Klaus Klein, September 14, 1999.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _inet_aton.c,v 1.4 2005/09/13 01:44:09 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(__indr_reference)
__indr_reference(_inet_aton, inet_aton)
#else

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int	_inet_aton(const char *, struct in_addr *);

int
inet_aton(const char *cp, struct in_addr *addr)
{

	return _inet_aton(cp, addr);
}
#endif
