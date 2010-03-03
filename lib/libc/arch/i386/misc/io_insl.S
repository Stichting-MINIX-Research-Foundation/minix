/*	insl() - Input a dword array			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void insl(U16_t port, void *buf, size_t count); */

.text
.globl	_insl
_insl:
	push	%ebp
	movl	%esp, %ebp
	cld
	push	%edi
	movl	8(%ebp), %edx	/* port */
	movl	12(%ebp), %edi	/* buf */
	movl	16(%ebp), %ecx	/* byte count */
	shrl	$2, %ecx	/* dword count */
	rep insl	/* input many dwords */
	pop	%edi
	pop	%ebp
	ret
