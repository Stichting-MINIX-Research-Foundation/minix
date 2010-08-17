/*	strncmp()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* int strncmp(const char *s1, const char *s2, size_t n) */
/*	Compare two strings. */
/* */
#include <machine/asm.h>

ENTRY(strncmp)
	movl	12(%esp), %ecx	/* Maximum length */
	jmp	_C_LABEL(_strncmp) /* Common code */
