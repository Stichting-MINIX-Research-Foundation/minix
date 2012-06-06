/*	$NetBSD: setgroupent.c,v 1.4 2003/10/27 00:12:43 lukem Exp $	*/

#include "nbtool_config.h"

#if !HAVE_SETGROUPENT || !HAVE_DECL_SETGROUPENT
#include <grp.h>

int setgroupent(int stayopen) {
	setgrent();
	return 1;
}
#endif
