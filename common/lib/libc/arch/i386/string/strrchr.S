/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strrchr.S,v 1.3 2014/03/22 19:38:46 jakllsch Exp $")
#endif

ENTRY(strrchr)
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	16(%esp),%edx
	movzbl	20(%esp),%ecx

	/* zero return value */
	xorl	%eax,%eax

	/*
	 * Align to word boundary.
	 * Consider unrolling loop?
	 */
.Lalign:
	testb	$3,%dl
	je	.Lword_aligned
	movb	(%edx),%bl
	cmpb	%cl,%bl
	jne	1f
	movl	%edx,%eax
1:	testb	%bl,%bl
	je	.Ldone
	incl	%edx
	jmp	.Lalign

.Lword_aligned:
	/* copy char to all bytes in word */
	movb	%cl,%ch
	movl	%ecx,%edi
	sall	$16,%ecx
	orl	%edi,%ecx

	/* Check whether any byte in the word is equal to ch or 0.  */
	_ALIGN_TEXT
.Lloop:
	movl	(%edx),%ebx
	addl	$4,%edx
	movl	%ebx,%esi
	leal	-0x01010101(%ebx),%edi
	xorl	%ecx,%esi
	subl	$0x01010101,%esi
	orl	%esi,%edi
	testl	$0x80808080,%edi
	je	.Lloop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word match
	 * ch or are equal to 0.
	 */

	_ALIGN_TEXT
	cmpb	%cl,%bl		/* 1st byte == ch? */
	jne	1f
	leal	-4(%edx),%eax
1:	testb	%bl,%bl		/* 1st byte == 0? */
	je	.Ldone

	cmpb	%cl,%bh		/* 2nd byte == ch? */
	jne	1f
	leal	-3(%edx),%eax
1:	testb	%bh,%bh		/* 2nd byte == 0? */
	je	.Ldone

	shrl	$16,%ebx
	cmpb	%cl,%bl		/* 3rd byte == ch? */
	jne	1f
	leal	-2(%edx),%eax
1:	testb	%bl,%bl		/* 3rd byte == 0? */
	je	.Ldone

	cmpb	%cl,%bh		/* 4th byte == ch? */
	jne	1f
	leal	-1(%edx),%eax
1:	testb	%bh,%bh		/* 4th byte == 0? */
	jne	.Lloop

.Ldone:
	popl	%ebx
	popl	%edi
	popl	%esi
	ret
END(strrchr)

STRONG_ALIAS(rindex,strrchr)
