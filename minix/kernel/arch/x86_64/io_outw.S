/*	outw() - Output one word			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outw(U16_t port, U16_t value); */
#include <machine/asm.h>

ENTRY(outw)
	movl	%esi, %eax	/* value */
	movl	%edi, %edx	/* port */
	outw	%dx		/* output 1 word from %ax */
	ret
