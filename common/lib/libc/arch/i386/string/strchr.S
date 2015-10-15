/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strchr.S,v 1.3 2014/03/22 19:38:46 jakllsch Exp $")
#endif

ENTRY(strchr)
	pushl	%esi
	pushl	%ebx
	movl	12(%esp),%eax
	movzbl	16(%esp),%ecx

	/*
	 * Align to word boundary.
	 * Consider unrolling loop?
	 */
.Lalign:
	testb	$3,%al
	je	.Lword_aligned
	movb	(%eax),%bl
	cmpb	%cl,%bl
	je	.Ldone
	testb	%bl,%bl
	je	.Lzero
	incl	%eax
	jmp	.Lalign

.Lword_aligned:
	/* copy char to all bytes in word */
	movb	%cl,%ch
	movl	%ecx,%edx
	sall	$16,%ecx
	orl	%edx,%ecx

	/* Check whether any byte in the word is equal to ch or 0. */
	_ALIGN_TEXT
.Lloop:
	movl	(%eax),%ebx
	addl	$4,%eax
	movl	%ebx,%esi
	leal	-0x01010101(%ebx),%edx
	xorl	%ecx,%esi
	subl	$0x01010101,%esi
	orl	%esi,%edx
	testl	$0x80808080,%edx
	je	.Lloop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word match
	 * ch or are equal to 0.
	 */

	/*
	 * Alignment here avoids a stall on the Athlon, even though
	 * it's not a branch target.
	 */

	_ALIGN_TEXT
	cmpb	%cl,%bl		/* 1st byte == ch? */
	jne	1f
	subl	$4,%eax
	jmp	.Ldone
1:	testb	%bl,%bl		/* 1st byte == 0? */
	je	.Lzero

	cmpb	%cl,%bh		/* 2nd byte == ch? */
	jne	1f
	subl	$3,%eax
	jmp	.Ldone
1:	testb	%bh,%bh		/* 2nd byte == 0? */
	je	.Lzero

	shrl	$16,%ebx
	cmpb	%cl,%bl		/* 3rd byte == ch? */
	jne	1f
	subl	$2,%eax
	jmp	.Ldone
1:	testb	%bl,%bl		/* 3rd byte == 0? */
	je	.Lzero

	cmpb	%cl,%bh		/* 4th byte == ch? */
	jne	1f
	decl	%eax
	jmp	.Ldone
1:	testb	%bh,%bh		/* 4th byte == 0? */
	jne	.Lloop

.Lzero:
	/* If a ch wasn't found, return 0. */
	xorl	%eax,%eax

.Ldone:
	popl	%ebx
	popl	%esi
	ret
END(strchr)

STRONG_ALIAS(index,strchr)
