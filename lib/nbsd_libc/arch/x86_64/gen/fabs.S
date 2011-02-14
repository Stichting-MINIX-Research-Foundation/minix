/*	$NetBSD: fabs.S,v 1.3 2004/03/23 17:11:35 drochner Exp $	*/

#include <machine/asm.h>
#if defined(LIBC_SCCS)
	RCSID("$NetBSD: fabs.S,v 1.3 2004/03/23 17:11:35 drochner Exp $")
#endif

	.section .rodata
	.align 8
__signmask:
	.long 0xffffffff
	.long 0x7fffffff

ENTRY(fabs)
	movsd	__signmask(%rip),%xmm1
	andpd	%xmm1,%xmm0
	ret
