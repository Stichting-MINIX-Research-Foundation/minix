/*	$NetBSD: fabs.S,v 1.4 2014/05/22 15:01:56 uebayasi Exp $	*/

#include <machine/asm.h>
#if defined(LIBC_SCCS)
	RCSID("$NetBSD: fabs.S,v 1.4 2014/05/22 15:01:56 uebayasi Exp $")
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
END(fabs)
