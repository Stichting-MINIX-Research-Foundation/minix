/*	$NetBSD: setpassent.c,v 1.4 2003/10/27 00:12:43 lukem Exp $	*/

#include "nbtool_config.h"

#if !HAVE_SETPASSENT || !HAVE_DECL_SETPASSENT
#include <pwd.h>

int setpassent(int stayopen) {
	setpwent();
	return 1;
}
#endif
