/*	$NetBSD$	*/

/*
 * strchrnul(3): like strchr(3), but return a pointer to the terminating
 * NUL instead of NULL when 'c' does not occur in 's'.  A GNU extension,
 * also in later NetBSD (10) and FreeBSD; added for MINIX's ported
 * software (Wayland compositors).
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <string.h>

char *
strchrnul(const char *s, int c)
{
	const char ch = (char)c;

	for (; *s != ch && *s != '\0'; s++)
		continue;
	return __UNCONST(s);
}
