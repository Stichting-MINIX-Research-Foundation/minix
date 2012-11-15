/*	$NetBSD: ntohl.c,v 1.3 2012/03/21 20:02:56 he Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ntohl.c,v 1.3 2012/03/21 20:02:56 he Exp $");
#endif

#include <sys/types.h>

#undef ntohl

uint32_t
ntohl(uint32_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (void *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}
