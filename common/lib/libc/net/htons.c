/*	$NetBSD: htons.c,v 1.1 2005/12/20 19:28:51 christos Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: htons.c,v 1.1 2005/12/20 19:28:51 christos Exp $");
#endif

#include <sys/types.h>

#undef htons

uint16_t
htons(x)
	uint16_t x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *) &x;
	return (uint16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}
