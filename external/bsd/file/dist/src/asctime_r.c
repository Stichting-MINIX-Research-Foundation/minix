/*	$NetBSD: asctime_r.c,v 1.1.1.1 2013/01/03 16:27:51 christos Exp $	*/

/*	$File: asctime_r.c,v 1.1 2012/05/15 17:14:36 christos Exp $	*/

#include "file.h"
#ifndef	lint
#if 0
FILE_RCSID("@(#)$File: asctime_r.c,v 1.1 2012/05/15 17:14:36 christos Exp $")
#else
__RCSID("$NetBSD: asctime_r.c,v 1.1.1.1 2013/01/03 16:27:51 christos Exp $");
#endif
#endif	/* lint */
#include <time.h>
#include <string.h>

/* asctime_r is not thread-safe anyway */
char *
asctime_r(const struct tm *t, char *dst)
{
	char *p = asctime(t);
	if (p == NULL)
		return NULL;
	memcpy(dst, p, 26);
	return dst;
}
