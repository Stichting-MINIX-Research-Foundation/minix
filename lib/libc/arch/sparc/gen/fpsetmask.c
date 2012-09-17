/*	$NetBSD: fpsetmask.c,v 1.6 2012/03/21 00:38:34 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetmask.c,v 1.6 2012/03/21 00:38:34 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetmask,_fpsetmask)
#endif

fp_except
fpsetmask(fp_except mask)
{
	fp_except old;
	fp_except new;

	__asm("st %%fsr,%0" : "=m" (*&old));

	new = old;
	new &= ~(0x1f << 23); 
	new |= ((mask & 0x1f) << 23);

	__asm("ld %0,%%fsr" : : "m" (*&new));

	return (old >> 23) & 0x1f;
}
