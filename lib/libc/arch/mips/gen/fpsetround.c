/*	$NetBSD: fpsetround.c,v 1.7 2014/09/17 11:02:55 joerg Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: fpsetround.c,v 1.7 2014/09/17 11:02:55 joerg Exp $");
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

	__asm(".set push; .set noat; cfc1 %0,$31; .set pop" : "=r" (old));

	new = old & ~0x03;
	new |= rnd_dir & 0x03;

	__asm(".set push; .set noat; ctc1 %0,$31; .set pop" : "=r" (new));

	return old & 0x03;
}
