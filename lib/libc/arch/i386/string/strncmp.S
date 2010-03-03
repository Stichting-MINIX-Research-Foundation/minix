/*	strncmp()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* int strncmp(const char *s1, const char *s2, size_t n) */
/*	Compare two strings. */
/* */
.text
.globl	_strncmp
.balign	16
_strncmp:
	movl	12(%esp), %ecx	/* Maximum length */
	jmp	__strncmp	/* Common code */
