/*	outb() - Output one byte			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outb(U16_t port, U8_t value); */
#include <machine/asm.h>

ENTRY(outb)
	movl	%esi, %eax	/* value */
	movl	%edi, %edx	/* port */
	outb	%dx		/* output 1 byte from %al */
	ret
