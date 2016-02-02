/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strcat.S,v 1.2 2014/03/22 19:38:46 jakllsch Exp $")
#endif

ENTRY(strcat)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	12(%esp),%eax

	/*
	 * Align destination to word boundary.
	 * Consider unrolling loop?
	 */
.Lscan:
.Lscan_align:
	testb	$3,%cl
	je	.Lscan_aligned
	cmpb	$0,(%ecx)
	je	.Lcopy
	incl	%ecx
	jmp	.Lscan_align

	_ALIGN_TEXT
.Lscan_aligned:
.Lscan_loop:
	movl	(%ecx),%ebx
	addl	$4,%ecx
	leal	-0x01010101(%ebx),%edx
	testl	$0x80808080,%edx
	je	.Lscan_loop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word equal 0.
	 */

	/*
	 * The optimal code for determining whether each byte is zero
	 * differs by processor.  This space-optimized code should be
	 * acceptable on all, especially since we don't expect it to
	 * be run frequently,
	 */

	testb	%bl,%bl		/* 1st byte == 0? */
	jne	1f
	subl	$4,%ecx
	jmp	.Lcopy

1:	testb	%bh,%bh		/* 2nd byte == 0? */
	jne	1f
	subl	$3,%ecx
	jmp	.Lcopy

1:	shrl	$16,%ebx
	testb	%bl,%bl		/* 3rd byte == 0? */
	jne	1f
	subl	$2,%ecx
	jmp	.Lcopy

1:	testb	%bh,%bh		/* 4th byte == 0? */
	jne	.Lscan_loop
	subl	$1,%ecx

	/*
	 * Align source to a word boundary.
	 * Consider unrolling loop?
	 */
.Lcopy:
.Lcopy_align:
	testl	$3,%eax
	je	.Lcopy_aligned
	movb	(%eax),%bl
	incl	%eax
	movb	%bl,(%ecx)
	incl	%ecx
	testb	%bl,%bl
	jne	.Lcopy_align
	jmp	.Ldone

	_ALIGN_TEXT
.Lcopy_loop:
	movl	%ebx,(%ecx)
	addl	$4,%ecx
.Lcopy_aligned:
	movl	(%eax),%ebx
	addl	$4,%eax
	leal	-0x01010101(%ebx),%edx
	testl	$0x80808080,%edx
	je	.Lcopy_loop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word equal 0.
	 */

	movb	%bl,(%ecx)
	incl	%ecx
	testb	%bl,%bl
	je	.Ldone

	movb	%bh,(%ecx)
	incl	%ecx
	testb	%bh,%bh
	je	.Ldone

	shrl	$16,%ebx
	movb	%bl,(%ecx)
	incl	%ecx
	testb	%bl,%bl
	je	.Ldone

	movb	%bh,(%ecx)
	incl	%ecx
	testb	%bh,%bh
	jne	.Lcopy_aligned

.Ldone:
	movl	8(%esp),%eax
	popl	%ebx
	ret
END(strcat)
