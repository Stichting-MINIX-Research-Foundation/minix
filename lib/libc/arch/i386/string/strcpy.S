/*	strcpy()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *strcpy(char *s1, const char *s2) */
/*	Copy string s2 to s1. */
/* */
#include <machine/asm.h>

ENTRY(strcpy)
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movl	$-1, %ecx	/* Unlimited length */
	call	_C_LABEL(_strncpy) /* Common code */
	movl	8(%ebp), %eax	/* Return s1 */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret
