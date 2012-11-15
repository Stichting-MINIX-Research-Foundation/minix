/*	$NetBSD: ldiv.S,v 1.9 2011/06/12 20:29:25 plunky Exp $	*/
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
RCSID("$NetBSD: ldiv.S,v 1.9 2011/06/12 20:29:25 plunky Exp $")
#endif

ENTRY(ldiv)
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
