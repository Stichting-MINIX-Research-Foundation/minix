/*	bcopy()						Author: Kees J. Bot */
/*								2 Jan 1994 */

/* void bcopy(const void *s1, void *s2, size_t n) */
/*	Copy a chunk of memory.  Handle overlap. */
/*	This is a BSD routine that escaped from the kernel.  Don't use. */
/* */
#include <machine/asm.h>

ENTRY(bcopy)
	movl	4(%esp), %eax	/* Exchange string arguments */
	xchgl	8(%esp), %eax
	movl	%eax, 4(%esp)
	jmp	_C_LABEL(_memmove) /* Call the proper routine */
