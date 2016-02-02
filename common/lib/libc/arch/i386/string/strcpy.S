/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strcpy.S,v 1.2 2014/03/22 19:38:46 jakllsch Exp $")
#endif

/*
 * This strcpy implementation copies a byte at a time until the
 * source pointer is aligned to a word boundary, it then copies by
 * words until it finds a word containing a zero byte, and finally
 * copies by bytes until the end of the string is reached.
 *
 * While this may result in unaligned stores if the source and
 * destination pointers are unaligned with respect to each other,
 * it is still faster than either byte copies or the overhead of
 * an implementation suitable for machines with strict alignment
 * requirements.
 */

ENTRY(strcpy)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	12(%esp),%eax

	/*
	 * Align source to a word boundary.
	 * Consider unrolling loop?
	 */
	_ALIGN_TEXT
.Lalign:
	testl	$3,%eax
	je	.Lword_aligned
	movb	(%eax),%bl
	incl	%eax
	movb	%bl,(%ecx)
	incl	%ecx
	testb	%bl,%bl
	jne	.Lalign
	jmp	.Ldone

	_ALIGN_TEXT
.Lloop:
	movl	%ebx,(%ecx)
	addl	$4,%ecx
.Lword_aligned:
	movl	(%eax),%ebx
	addl	$4,%eax
	leal	-0x01010101(%ebx),%edx
	testl	$0x80808080,%edx
	je	.Lloop

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
	jne	.Lword_aligned

.Ldone:
	movl	8(%esp),%eax
	popl	%ebx
	ret
END(strcpy)
