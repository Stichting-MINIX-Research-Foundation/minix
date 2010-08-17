/*	_strncpy()					Author: Kees J. Bot */
/*								1 Jan 1994 */

/* char *_strncpy(char *s1, const char *s2, size_t ecx) */
/*	Copy string s2 to s1. */
/* */
#include <machine/asm.h>

ENTRY(_strncpy)
	movl	12(%ebp), %edi	/* edi = string s2 */
	xorb	%al, %al	/* Look for a zero byte */
	movl	%ecx, %edx	/* Save maximum count */
	cld

	repne scasb	/* Look for end of s2 */
	subl	%ecx, %edx	/* Number of bytes in s2 including null */
	xchgl	%edx, %ecx
	movl	12(%ebp), %esi	/* esi = string s2 */
	movl	8(%ebp), %edi	/* edi = string s1 */

	rep movsb	/* Copy bytes */
	ret
