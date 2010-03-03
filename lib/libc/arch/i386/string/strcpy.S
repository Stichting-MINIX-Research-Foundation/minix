/*	strcpy()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *strcpy(char *s1, const char *s2) */
/*	Copy string s2 to s1. */
/* */
.text
.globl	_strcpy
.balign	16
_strcpy:
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movl	$-1, %ecx	/* Unlimited length */
	call	__strncpy	/* Common code */
	movl	8(%ebp), %eax	/* Return s1 */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret
