/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_scalbn.S,v 1.14 2013/05/20 19:40:09 joerg Exp $")

#ifdef WEAK_ALIAS
WEAK_ALIAS(scalbn,_scalbn)
WEAK_ALIAS(scalbln,_scalbln)
WEAK_ALIAS(ldexp,_scalbn)
#endif
#ifdef __x86_64__
ENTRY(_scalbn)
	movl %edi,-12(%rsp)
	fildl -12(%rsp)
	movsd %xmm0,-8(%rsp)
	fldl -8(%rsp)
	fscale
	fstpl -8(%rsp)
	movsd -8(%rsp),%xmm0
	fstp %st(0)
	ret
#elif defined(STRONG_ALIAS)
STRONG_ALIAS(_scalbn, scalbln)
#endif

ENTRY(_scalbln)
#ifdef __x86_64__
	movq %rdi,-16(%rsp)
	fildq -16(%rsp)
	movsd %xmm0,-8(%rsp)
	fldl -8(%rsp)
	fscale
	fstpl -8(%rsp)
	movsd -8(%rsp),%xmm0
	fstp %st(0)
#else
	fildl 12(%esp)
	fldl 4(%esp)
	fscale
	fstp %st(1)		/* clean up stack */
#endif
	ret
