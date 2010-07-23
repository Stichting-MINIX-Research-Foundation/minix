/*	inb() - Input one byte				Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	unsigned inb(U16_t port); */

.text
.globl	_inb
_inb:
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %edx	/* port */
	xorl	%eax, %eax
	inb	%dx	/* read 1 byte */
	pop	%ebp
	ret
