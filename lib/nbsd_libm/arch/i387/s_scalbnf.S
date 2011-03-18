/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_scalbnf.S,v 1.8 2010/04/23 19:17:07 drochner Exp $")

#ifdef WEAK_ALIAS
WEAK_ALIAS(scalbnf,_scalbnf)
#endif

ENTRY(_scalbnf)
#ifdef __x86_64__
	movl %edi,-8(%rsp)
	fildl -8(%rsp)
	movss %xmm0,-4(%rsp)
	flds -4(%rsp)
	fscale
	fstps -4(%rsp)
	movss -4(%rsp),%xmm0
	fstp %st(0)
#else
	fildl 8(%esp)
	flds 4(%esp)
	fscale
	fstp %st(1)		/* clean up stack */
#endif
	ret
