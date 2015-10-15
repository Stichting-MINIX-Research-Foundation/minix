/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
        RCSID("$NetBSD: strncmp.S,v 1.4 2014/05/22 15:01:57 uebayasi Exp $")
#endif

/*
 * NOTE: I've unrolled the loop eight times: large enough to make a
 * significant difference, and small enough not to totally trash the
 * cache.
 */

ENTRY(strncmp)
	testq	%rdx,%rdx
	jmp	L2			/* Jump into the loop! */

L1:	incq	%rdi
	incq	%rsi
	decq	%rdx
L2:	jz	L4			/* strings are equal */
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	jne	L3

	incq	%rdi
	incq	%rsi
	decq	%rdx
	jz	L4
	movb	(%rdi),%al
	testb	%al,%al
	jz	L3
	cmpb	%al,(%rsi)
	je	L1

L3:	movzbl	(%rdi),%eax		/* unsigned comparison */
	movzbl	(%rsi),%ecx
	subl	%ecx,%eax
	ret
L4:	xorl	%eax,%eax
	ret
END(strncmp)
