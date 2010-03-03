/*	strnlen()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t strnlen(const char *s, size_t n) */
/*	Return the length of a string. */
/* */
.text
.globl	_strnlen
.balign	16
_strnlen:
	movl	8(%esp), %ecx	/* Maximum length */
	jmp	__strnlen	/* Common code */
