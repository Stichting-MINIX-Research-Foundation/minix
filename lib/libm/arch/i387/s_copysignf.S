/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXXfvdl split this file.
 */

RCSID("$NetBSD: s_copysignf.S,v 1.7 2011/06/21 21:52:49 joerg Exp $")

#ifdef __x86_64__
.Lneg:
	.long 0x7fffffff
.Lpos:
	.long 0x80000000
#endif

ENTRY(copysignf)
#ifdef __i386__
	movl	8(%esp),%edx
	andl	$0x80000000,%edx
	movl	4(%esp),%eax
	andl	$0x7fffffff,%eax
	orl	%edx,%eax
	movl	%eax,4(%esp)
	flds	4(%esp)
#else
	movss	.Lpos(%rip),%xmm2
	movss	.Lneg(%rip),%xmm3
	pand	%xmm2,%xmm1
	pand	%xmm3,%xmm0
	por	%xmm1,%xmm0
#endif
	ret
