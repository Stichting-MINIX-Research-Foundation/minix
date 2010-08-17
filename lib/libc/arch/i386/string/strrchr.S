/*	strrchr()					Author: Kees J. Bot */
/*								2 Jan 1994 */

/* char *strrchr(const char *s, int c) */
/*	Look for the last occurrence a character in a string. */
/* */
#include <machine/asm.h>

ENTRY(strrchr)
	push	%ebp
	movl	%esp, %ebp
	push	%edi
	movl	8(%ebp), %edi	/* edi = string */
	movl	$-1, %ecx
	xorb	%al, %al
	cld

	repne scasb	/* Look for the end of the string */
	notl	%ecx	/* -1 - ecx = Length of the string + null */
	decl	%edi	/* Put edi back on the zero byte */
	movb	12(%ebp), %al	/* The character to look for */
	std	/* Downwards search */

	repne scasb
	cld	/* Direction bit back to default */
	jne	failure
	leal	1(%edi), %eax	/* Found it */
	pop	%edi
	pop	%ebp
	ret
failure:
	xorl	%eax, %eax	/* Not there */
	pop	%edi
	pop	%ebp
	ret
