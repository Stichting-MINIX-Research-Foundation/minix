/*	$NetBSD: issetugid.c,v 1.2 2003/10/27 00:12:43 lukem Exp $	*/

/*
 * Written by Ben Harris, 2002
 * This file is in the Public Domain
 */

#include "nbtool_config.h"

#if !HAVE_ISSETUGID
int
issetugid(void)
{

	/*
	 * Assume that anything linked against libnbcompat will be installed
	 * without special privileges.
	 */
	return 0;
}
#endif
