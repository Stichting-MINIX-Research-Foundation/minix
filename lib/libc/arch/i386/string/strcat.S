/*	strcat()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *strcat(char *s1, const char *s2) */
/*	Append string s2 to s1. */
/* */
#include <machine/asm.h>

ENTRY(strcat)
	movl	$-1, %edx	/* Unlimited length */
	jmp	_C_LABEL(_strncat) /* Common code */
