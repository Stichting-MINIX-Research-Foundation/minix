/*	strncat()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t strncat(char *s1, const char *s2, size_t n) */
/*	Append string s2 to s1. */
/* */
.text
.globl	_strncat
.balign	16
_strncat:
	movl	12(%esp), %edx	/* Maximum length */
	jmp	__strncat	/* Common code */
