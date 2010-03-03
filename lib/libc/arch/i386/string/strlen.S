/*	strlen()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t strlen(const char *s) */
/*	Return the length of a string. */
/* */
.text
.globl	_strlen
.balign	16
_strlen:
	movl	$-1, %ecx	/* Unlimited length */
	jmp	__strnlen	/* Common code */
