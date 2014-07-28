/*	outb() - Output one byte			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outb(U16_t port, U8_t value); */
#include <machine/asm.h>

ENTRY(outb)
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %edx	/* port */
	movl	8+4(%ebp), %eax	/* value */
	outb	%dx	/* output 1 byte */
	pop	%ebp
	ret
