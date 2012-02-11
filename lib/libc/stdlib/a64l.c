/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: a64l.c,v 1.9 2003/07/26 19:24:53 salo Exp $");
#endif

#include "namespace.h"

#include <assert.h>
#include <stdlib.h>

#ifdef __weak_alias
__weak_alias(a64l,_a64l)
#endif

long
a64l(s)
	const char *s;
{
	long value, digit, shift;
	int i;

	_DIAGASSERT(s != NULL);

	value = 0;
	shift = 0;
	for (i = 0; *s && i < 6; i++, s++) {
		if (*s <= '/')
			digit = *s - '.';
		else if (*s <= '9')
			digit = *s - '0' + 2;
		else if (*s <= 'Z')
			digit = *s - 'A' + 12;
		else
			digit = *s - 'a' + 38; 

		value |= digit << shift;
		shift += 6;
	}

	return (long) value;
}
