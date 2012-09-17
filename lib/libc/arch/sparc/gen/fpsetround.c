/*	$NetBSD: fpsetround.c,v 1.6 2012/03/21 00:38:35 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetround.c,v 1.6 2012/03/21 00:38:35 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpsetround,_fpsetround)
#endif

fp_rnd
fpsetround(fp_rnd rnd_dir)
{
	fp_rnd old;
	fp_rnd new;

	__asm("st %%fsr,%0" : "=m" (*&old));

	new = old;
	new &= ~(0x03 << 30); 
	new |= ((rnd_dir & 0x03) << 30);

	__asm("ld %0,%%fsr" : : "m" (*&new));

	return ((unsigned int)old >> 30) & 0x03;
}
