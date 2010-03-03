/*	strcmp()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* int strcmp(const char *s1, const char *s2) */
/*	Compare two strings. */
/* */
.text
.globl	_strcmp
.balign	16
_strcmp:
	movl	$-1, %ecx	/* Unlimited length */
	jmp	__strncmp	/* Common code */
