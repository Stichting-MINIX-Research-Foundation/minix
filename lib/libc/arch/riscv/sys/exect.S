/*	$NetBSD: exect.S,v 1.1 2014/09/19 17:36:25 matt Exp $	*/

#include "SYS.h"

#if defined(LIBC_SCCS) && !defined(lint)
	RCSID("$NetBSD: exect.S,v 1.1 2014/09/19 17:36:25 matt Exp $")
#endif /* LIBC_SCCS and not lint */

PSEUDO(exect, execve)
