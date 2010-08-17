/*	strncat()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t strncat(char *s1, const char *s2, size_t n) */
/*	Append string s2 to s1. */
/* */
#include <machine/asm.h>

ENTRY(strncat)
	movl	12(%esp), %edx	/* Maximum length */
	jmp	_C_LABEL(_strncat) /* Common code */
