#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: bcmp.S,v 1.2 2007/11/12 18:41:59 ad Exp $")
#endif

ENTRY(bcmp)
	xorl	%eax,%eax		/* clear return value */

	movq	%rdx,%rcx		/* compare by words */
	shrq	$3,%rcx
	repe
	cmpsq
	jne	L1

	movq	%rdx,%rcx		/* compare remainder by bytes */
	andq	$7,%rcx
	repe
	cmpsb
	je	L2

L1:	incl	%eax
L2:	ret
