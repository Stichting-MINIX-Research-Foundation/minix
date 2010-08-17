/*	_strnlen()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t _strnlen(const char *s, size_t ecx) */
/*	Return the length of a string. */
/* */
#include <machine/asm.h>

ENTRY(_strnlen)
	push	%ebp
	movl	%esp, %ebp
	push	%edi
	movl	8(%ebp), %edi	/* edi = string */
	xorb	%al, %al	/* Look for a zero byte */
	movl	%ecx, %edx	/* Save maximum count */
	cmpb	$1, %cl	/* 'Z' bit must be clear if ecx = 0 */
	cld

	repne scasb	/* Look for zero */
	jne	no0
	incl	%ecx	/* Don't count zero byte */
no0:
	movl	%edx, %eax
	subl	%ecx, %eax	/* Compute bytes scanned */
	pop	%edi
	pop	%ebp
	ret
