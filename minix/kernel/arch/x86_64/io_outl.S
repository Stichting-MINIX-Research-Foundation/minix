/*	outl() - Output one dword			Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void outl(U16_t port, u32_t value); */
#include <machine/asm.h>

ENTRY(outl)
	movl	%esi, %eax	/* value */
	movl	%edi, %edx	/* port */
	outl	%dx		/* output 1 dword from %eax */
	ret
