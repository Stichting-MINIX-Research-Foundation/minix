/*	strchr()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *strchr(const char *s, int c) */
/*	Look for a character in a string. */
/* */
#include <machine/asm.h>

ENTRY(strchr)
	push	%ebp
	movl	%esp, %ebp
	push	%edi
	cld
	movl	8(%ebp), %edi	/* edi = string */
	movl	$16, %edx	/* Look at small chunks of the string */
next:
	shll	$1, %edx	/* Chunks become bigger each time */
	movl	%edx, %ecx
	xorb	%al, %al	/* Look for the zero at the end */

	repne scasb
	pushf	/* Remember the flags */
	subl	%edx, %ecx
	negl	%ecx	/* Some or all of the chunk */
	subl	%ecx, %edi	/* Step back */
	movb	12(%ebp), %al	/* The character to look for */

	repne scasb
	je	found
	popf	/* Did we find the end of string earlier? */
	jne	next	/* No, try again */
	xorl	%eax, %eax	/* Return NULL */
	pop	%edi
	pop	%ebp
	ret
found:
	pop	%eax	/* Get rid of those flags */
	leal	-1(%edi), %eax	/* Address of byte found */
	pop	%edi
	pop	%ebp
	ret
