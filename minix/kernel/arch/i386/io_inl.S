/*	inl() - Input one dword				Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	unsigned inl(U16_t port); */
#include <machine/asm.h>

ENTRY(inl)
	push	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %edx	/* port */
	inl	%dx	/* read 1 dword */
	pop	%ebp
	ret
