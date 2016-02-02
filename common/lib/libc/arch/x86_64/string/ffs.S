/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: ffs.S,v 1.5 2014/03/22 19:16:34 jakllsch Exp $")
#endif

ENTRY(ffs)
	bsfl	%edi,%eax
	jz	1f	 		/* ZF is set if all bits are 0 */
	incl	%eax			/* bits numbered from 1, not 0 */
	ret

1:	xorl	%eax,%eax		/* clear result */
	ret
END(ffs)
