/*	$NetBSD: swab.S,v 1.11 2014/03/18 18:20:37 riastradh Exp $	*/

#include <machine/asm.h>

ENTRY(swab)
	movl	4(%sp),%a0		| source
	movl	8(%sp),%a1		| destination
	movl	12(%sp),%d0		| count
	lsrl	#1,%d0			| count is in bytes; we need words
	jeq	swdone

swloop:
#ifdef __mcoldfire__
	movb	(%a0)+,1(%a1)
	movb	(%a0)+,(%a1)
	addql	#2,%a1
#else
	movw	(%a0)+,%d1
	rorw	#8,%d1
	movw	%d1,(%a1)+
#endif
	subql	#1,%d0
	jne	swloop

swdone:
	rts
END(swab)
