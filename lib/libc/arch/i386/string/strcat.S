/*	strcat()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *strcat(char *s1, const char *s2) */
/*	Append string s2 to s1. */
/* */
.text
.globl	_strcat
.balign	16
_strcat:
	movl	$-1, %edx	/* Unlimited length */
	jmp	__strncat	/* Common code */
