/*	$NetBSD: fpgetround.c,v 1.6 2012/06/24 15:26:02 christos Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetround.c,v 1.6 2012/06/24 15:26:02 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/types.h>
#include <ieeefp.h>

#ifdef __weak_alias
__weak_alias(fpgetround,_fpgetround)
#endif

fp_rnd
fpgetround(void)
{
	uint32_t x;

	__asm("st %%fsr,%0" : "=m" (*&x));
	return (x >> 30) & 0x03;
}
