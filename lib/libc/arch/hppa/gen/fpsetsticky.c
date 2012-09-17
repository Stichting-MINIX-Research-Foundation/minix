/*	$NetBSD: fpsetsticky.c,v 1.6 2012/03/23 09:34:09 skrll Exp $	*/

/*	$OpenBSD: fpsetsticky.c,v 1.4 2004/01/05 06:06:16 otto Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetsticky.c,v 1.6 2012/03/23 09:34:09 skrll Exp $");
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpsetsticky(fp_except mask)
{
	uint64_t fpsr;
	fp_except old;

	__asm volatile("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr) : "memory");
	old = (fp_except)(fpsr >> 59) & 0x1f;
	fpsr = (fpsr & 0x07ffffff00000000LL) | ((uint64_t)(mask & 0x1f) << 59);
	__asm volatile("fldd 0(%0),%%fr0" : : "r" (&fpsr) : "memory");

	return (old);
}
