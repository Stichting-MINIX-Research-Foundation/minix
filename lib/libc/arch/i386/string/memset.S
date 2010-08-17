/*	memset()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* void *memset(void *s, int c, size_t n) */
/*	Set a chunk of memory to the same byte value. */
/* */
#include <machine/asm.h>

ENTRY(memset)
	push	%ebp
	movl	%esp, %ebp
	push	%edi
	movl	8(%ebp), %edi	/* The string */
	movzbl	12(%ebp), %eax	/* The fill byte */
	movl	16(%ebp), %ecx	/* Length */
	cld
	cmpl	$16, %ecx
	jb	sbyte	/* Don't bother being smart with short arrays */
	testl	$1, %edi
	jne	sbyte	/* Bit 0 set, use byte store */
	testl	$2, %edi
	jne	sword	/* Bit 1 set, use word store */
slword:
	movb	%al, %ah
	movl	%eax, %edx
	sall	$16, %edx
	orl	%edx, %eax	/* One byte to four bytes */
	shrdl	$2, %ecx, %edx	/* Save low two bits of ecx in edx */
	shrl	$2, %ecx

	rep stosl	/* Store longwords. */
	shldl	$2, %edx, %ecx	/* Restore low two bits */
sword:
	movb	%al, %ah	/* One byte to two bytes */
	shrl	$1, %ecx

	rep stosw	/* Store words */
	adcl	%ecx, %ecx	/* One more byte? */
sbyte:
	rep stosb	/* Store bytes */
done:
	movl	8(%ebp), %eax	/* Return some value you have no need for */
	pop	%edi
	pop	%ebp
	ret
