/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: bcmp.S,v 1.9 2007/11/12 18:41:59 ad Exp $")
#endif

ENTRY(bcmp)
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%edi
	movl	16(%esp),%esi
	xorl	%eax,%eax		/* clear return value */

	movl	20(%esp),%ecx		/* compare by words */
	shrl	$2,%ecx
	repe
	cmpsl
	jne	L1

	movl	20(%esp),%ecx		/* compare remainder by bytes */
	andl	$3,%ecx
	repe
	cmpsb
	je	L2

L1:	incl	%eax
L2:	popl	%esi
	popl	%edi
	ret
