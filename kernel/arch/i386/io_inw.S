/*	inw() - Input one word				Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	unsigned inw(U16_t port); */
#include <machine/asm.h>

ENTRY(inw)
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %edx	/* port */
	xorl	%eax, %eax
	inw	%dx	/* read 1 word */
	pop	%ebp
	ret
