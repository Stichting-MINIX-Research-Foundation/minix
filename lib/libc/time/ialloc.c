/*	$NetBSD: ialloc.c,v 1.9 2012/10/26 18:29:34 christos Exp $	*/
/*
** This file is in the public domain, so clarified as of
** 2006-07-17 by Arthur David Olson.
*/

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

#if 0
static char	elsieid[] = "@(#)ialloc.c	8.30";
#else
__RCSID("$NetBSD: ialloc.c,v 1.9 2012/10/26 18:29:34 christos Exp $");
#endif

#include "private.h"

char *
icatalloc(char *const old, const char *const new)
{
	char *	result;
	int	oldsize, newsize;

	newsize = (new == NULL) ? 0 : strlen(new);
	if (old == NULL)
		oldsize = 0;
	else if (newsize == 0)
		return old;
	else
		oldsize = strlen(old);
	if ((result = realloc(old, oldsize + newsize + 1)) != NULL)
		if (new != NULL)
			(void) strcpy(result + oldsize, new); /* XXX strcpy is safe */
	return result;
}

char *
icpyalloc(const char *const string)
{
	return icatalloc(NULL, string);
}
