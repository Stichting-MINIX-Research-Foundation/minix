/*	memchr()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* void *memchr(const void *s, int c, size_t n) */
/*	Look for a character in a chunk of memory. */
/* */
#include <machine/asm.h>

ENTRY(memchr)
	push	%ebp
	movl	%esp, %ebp
	push	%edi
	movl	8(%ebp), %edi	/* edi = string */
	movb	12(%ebp), %al	/* The character to look for */
	movl	16(%ebp), %ecx	/* Length */
	cmpb	$1, %cl	/* 'Z' bit must be clear if ecx = 0 */
	cld

	repne scasb
	jne	failure
	leal	-1(%edi), %eax	/* Found */
	pop	%edi
	pop	%ebp
	ret
failure:
	xorl	%eax, %eax
	pop	%edi
	pop	%ebp
	ret
