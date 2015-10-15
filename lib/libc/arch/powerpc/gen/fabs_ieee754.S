/*	$NetBSD: fabs_ieee754.S,v 1.1 2014/01/28 13:47:04 macallan Exp $	*/

#include <machine/asm.h>

#if defined(LIBC_SCCS)
__RCSID("$NetBSD: fabs_ieee754.S,v 1.1 2014/01/28 13:47:04 macallan Exp $")
#endif

ENTRY(fabs)
	fabs %f1,%f1
	blr
END(fabs)
