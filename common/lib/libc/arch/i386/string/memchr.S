/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: memchr.S,v 1.1 2005/12/20 19:28:49 christos Exp $")
#endif

ENTRY(memchr)
	pushl	%esi
	movl	8(%esp),%eax
	movzbl	12(%esp),%ecx
	movl	16(%esp),%esi

	/*
	 * Align to word boundary. 
	 * Consider unrolling loop?
	 */
	testl	%esi,%esi	/* nbytes == 0? */
	je	.Lzero
.Lalign:
	testb	$3,%al
	je	.Lword_aligned
	cmpb	(%eax),%cl
	je	.Ldone
	incl	%eax
	decl	%esi
	jnz	.Lalign
	jmp	.Lzero

.Lword_aligned:
	/* copy char to all bytes in word */
	movb	%cl,%ch
	movl	%ecx,%edx
	sall	$16,%ecx
	orl	%edx,%ecx

	_ALIGN_TEXT
.Lloop:
	cmpl	$3,%esi		/* nbytes > 4 */
	jbe	.Lbyte
	movl	(%eax),%edx
	addl	$4,%eax
	xorl	%ecx,%edx
	subl	$4,%esi
	subl	$0x01010101,%edx
	testl	$0x80808080,%edx
	je	.Lloop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word are
	 * equal to ch.
	 */

	/*
	 * High load-use latency on the Athlon leads to significant
	 * stalls, so we preload the next char as soon as possible
	 * instead of using cmp mem8, reg8.
	 *
	 * Alignment here avoids a stall on the Athlon, even though
	 * it's not a branch target.
	 */
	_ALIGN_TEXT
	cmpb	-4(%eax),%cl	/* 1st byte == ch? */
	movb	-3(%eax),%dl
	jne	1f
	subl	$4,%eax
	jmp	.Ldone

	_ALIGN_TEXT
1:	cmpb	%dl,%cl		/* 2nd byte == ch? */
	movb	-2(%eax),%dl
	jne	1f
	subl	$3,%eax
	jmp	.Ldone

	_ALIGN_TEXT
1:	cmpb	%dl,%cl		/* 3rd byte == ch? */
	movb	-1(%eax),%dl
	jne	1f
	subl	$2,%eax
	jmp	.Ldone

	_ALIGN_TEXT
1:	cmpb	%dl,%cl		/* 4th byte == ch? */
	jne	.Lloop
	decl	%eax
	jmp	.Ldone

.Lbyte:
	testl	%esi,%esi
	je	.Lzero
.Lbyte_loop:
	cmpb	(%eax),%cl
	je	.Ldone
	incl	%eax
	decl	%esi
	jnz	.Lbyte_loop

.Lzero:
	xorl	%eax,%eax

.Ldone:
	popl	%esi
	ret
