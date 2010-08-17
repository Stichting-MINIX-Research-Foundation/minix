/*	strlen()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* size_t strlen(const char *s) */
/*	Return the length of a string. */
/* */
#include <machine/asm.h>

ENTRY(strlen)
	movl	$-1, %ecx	/* Unlimited length */
	jmp	_C_LABEL(_strnlen) /* Common code */
