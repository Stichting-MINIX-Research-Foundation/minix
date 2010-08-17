/*	strncpy()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *strncpy(char *s1, const char *s2, size_t n) */
/*	Copy string s2 to s1. */
/* */
#include <machine/asm.h>

ENTRY(strncpy)
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movl	16(%ebp), %ecx	/* Maximum length */
	call	_C_LABEL(_strncpy) /* Common code */
	movl	%edx, %ecx	/* Number of bytes not copied */

	rep stosb	/* strncpy always copies n bytes by null padding */
	movl	8(%ebp), %eax	/* Return s1 */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret
