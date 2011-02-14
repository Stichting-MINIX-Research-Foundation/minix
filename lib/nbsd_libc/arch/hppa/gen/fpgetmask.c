/*	$NetBSD: fpgetmask.c,v 1.4 2005/12/24 21:42:32 perry Exp $	*/

/*	$OpenBSD: fpgetmask.c,v 1.3 2002/10/21 18:41:05 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpgetmask.c,v 1.4 2005/12/24 21:42:32 perry Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpgetmask(void)
{
	uint64_t fpsr;

	__asm volatile("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	return ((fpsr >> 32) & 0x1f);
}
