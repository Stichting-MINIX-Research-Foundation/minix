/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strcmp.S,v 1.1 2005/12/20 19:28:49 christos Exp $")
#endif

ENTRY(strcmp)
	pushl	%esi
	pushl	%ebx
	movl	12(%esp),%ebx
	movl	16(%esp),%esi

	/*
	 * Align s1 to word boundary.
	 * Consider unrolling loop?
	 */
.Ls1align:
	testb	$3,%bl
	je	.Ls1aligned
	movb	(%ebx),%al
	incl	%ebx
	movb	(%esi),%dl
	incl	%esi
	testb	%al,%al
	je	.Ldone
	cmpb	%al,%dl
	je	.Ls1align
	jmp	.Ldone

	/*
	 * Check whether s2 is aligned to a word boundary.  If it is, we
	 * can compare by words.  Otherwise we have to compare by bytes.
	 */
.Ls1aligned:
	testl	$3,%esi
	jne	.Lbyte_loop

	subl	$4,%ebx
	subl	$4,%esi

	_ALIGN_TEXT
.Lword_loop:
	movl	4(%ebx),%eax
	addl	$4,%ebx
	movl	4(%esi),%edx
	addl	$4,%esi
	cmpl	%eax,%edx
	jne	.Lbyte_loop
	subl	$0x01010101,%edx
	notl	%eax
	andl	%eax,%edx
	testl	$0x80808080,%edx
	je	.Lword_loop

	_ALIGN_TEXT
.Lbyte_loop:
	movb	(%ebx),%al
	incl	%ebx
	movb	(%esi),%dl
	incl	%esi
	testb	%al,%al
	je	.Ldone
	cmpb	%al,%dl
	je	.Lbyte_loop

.Ldone:
	movzbl	%al,%eax
	movzbl	%dl,%edx
	subl	%edx,%eax
	popl	%ebx
	popl	%esi
	ret
