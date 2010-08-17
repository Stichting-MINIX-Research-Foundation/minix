/*	outw() - Output one word			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outw(U16_t port, U16_t value); */
#include <machine/asm.h>

ENTRY(outw)
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %edx	/* port */
	movl	8+4(%ebp), %eax	/* value */
	outw	%dx	/* output 1 word */
	pop	%ebp
	ret
