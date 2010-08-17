/*	outl() - Output one dword			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outl(U16_t port, u32_t value); */
#include <machine/asm.h>

ENTRY(outl)
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %edx	/* port */
	movl	8+4(%ebp), %eax	/* value */
	outl	%dx	/* output 1 dword */
	pop	%ebp
	ret
