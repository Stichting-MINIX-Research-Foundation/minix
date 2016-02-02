/*	$NetBSD: div.S,v 1.10 2014/05/23 02:34:19 uebayasi Exp $	*/
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
RCSID("$NetBSD: div.S,v 1.10 2014/05/23 02:34:19 uebayasi Exp $")
#endif

ENTRY(div)
	pushl	%ebx
	movl	8(%esp),%ebx
	movl	12(%esp),%eax
	movl	16(%esp),%ecx
	cdq
	idiv	%ecx
	movl	%eax,(%ebx)
	movl	%edx,4(%ebx)
	movl	%ebx,%eax
	popl	%ebx
	ret	$4
END(div)
