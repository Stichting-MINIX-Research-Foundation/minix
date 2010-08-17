/*	strnlen()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t strnlen(const char *s, size_t n) */
/*	Return the length of a string. */
/* */
#include <machine/asm.h>

ENTRY(strnlen)
	movl	8(%esp), %ecx	/* Maximum length */
	jmp	_C_LABEL(_strnlen) /* Common code */
