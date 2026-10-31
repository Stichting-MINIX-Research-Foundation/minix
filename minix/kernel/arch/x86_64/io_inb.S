/*	inb() - Input one byte				Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	unsigned inb(U16_t port); */
#include <machine/asm.h>

ENTRY(inb)
	movl	%edi, %edx	/* port */
	xorl	%eax, %eax
	inb	%dx		/* read 1 byte into %al */
	ret
