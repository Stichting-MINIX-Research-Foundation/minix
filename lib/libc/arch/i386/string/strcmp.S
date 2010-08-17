/*	strcmp()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* int strcmp(const char *s1, const char *s2) */
/*	Compare two strings. */
/* */
#include <machine/asm.h>

ENTRY(strcmp)
	movl	$-1, %ecx	/* Unlimited length */
	jmp	_C_LABEL(_strncmp) /* Common code */
