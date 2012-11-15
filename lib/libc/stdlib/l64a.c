/*	$NetBSD: l64a.c,v 1.14 2012/03/13 21:13:48 christos Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: l64a.c,v 1.14 2012/03/13 21:13:48 christos Exp $");
#endif

#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#ifdef __weak_alias
__weak_alias(l64a,_l64a)
__weak_alias(l64a_r,_l64a_r)
#endif

char *
l64a(long value)
{
	static char buf[8];

	(void)l64a_r(value, buf, (int)sizeof (buf));
	return buf;
}

int
l64a_r(long value, char *buffer, int buflen)
{
	char *s = buffer;
	int digit;
	unsigned long v = value;

	_DIAGASSERT(buffer != NULL);

	if (value == 0UL) 
		goto out;

	for (; v != 0 && buflen > 1; s++, buflen--) {
		digit = (int)(v & 0x3f);

		if (digit < 2) 
			*s = digit + '.';
		else if (digit < 12)
			*s = digit + '0' - 2;
		else if (digit < 38)
			*s = digit + 'A' - 12;
		else
			*s = digit + 'a' - 38;
		v >>= 6;
	}

out:
	*s = '\0';

	return (v == 0UL ? 0 : -1);
}
