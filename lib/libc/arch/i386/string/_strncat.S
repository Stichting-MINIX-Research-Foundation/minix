/*	_strncat()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *_strncat(char *s1, const char *s2, size_t edx) */
/*	Append string s2 to s1. */
/* */
#include <machine/asm.h>

ENTRY(_strncat)
	push	%ebp
	movl	%esp, %ebp
	push	%esi
	push	%edi
	movl	8(%ebp), %edi	/* String s1 */
	movl	$-1, %ecx
	xorb	%al, %al	/* Null byte */
	cld

	repne scasb	/* Look for the zero byte in s1 */
	decl	%edi	/* Back one up (and clear 'Z' flag) */
	push	%edi	/* Save end of s1 */
	movl	12(%ebp), %edi	/* edi = string s2 */
	movl	%edx, %ecx	/* Maximum count */

	repne scasb	/* Look for the end of s2 */
	jne	no0
	incl	%ecx	/* Exclude null byte */
no0:
	subl	%ecx, %edx	/* Number of bytes in s2 */
	movl	%edx, %ecx
	movl	12(%ebp), %esi	/* esi = string s2 */
	pop	%edi	/* edi = end of string s1 */

	rep movsb	/* Copy bytes */
	stosb	/* Add a terminating null */
	movl	8(%ebp), %eax	/* Return s1 */
	pop	%edi
	pop	%esi
	pop	%ebp
	ret
