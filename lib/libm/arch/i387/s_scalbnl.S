/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_scalbnl.S,v 1.5 2013/05/20 20:27:43 mlelstv Exp $")

#ifdef WEAK_ALIAS
WEAK_ALIAS(scalbnl,_scalbnl)
WEAK_ALIAS(scalblnl,_scalblnl)
WEAK_ALIAS(ldexpl,_scalbnl)
#endif

#ifdef __x86_64__
ENTRY(_scalbnl)
	movl	%edi,-4(%rsp)
	fildl	-4(%rsp)
	fldt	8(%rsp)
	fscale
	fstp	%st(1)
	ret
#elif defined(STRONG_ALIAS)
STRONG_ALIAS(_scalbnl,_scalblnl)
#endif

ENTRY(_scalblnl)
#ifdef __x86_64__
	movq	%rdi,-8(%rsp)
	fildq	-8(%rsp)
	fldt	8(%rsp)
	fscale
	fstp	%st(1)
#else
	fildl 16(%esp)
	fldt 4(%esp)
	fscale
	fstp %st(1)		/* clean up stack */
#endif
	ret
