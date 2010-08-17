/*	bzero()						Author: Kees J. Bot */
/*								2 Jan 1994 */

/* void bzero(void *s, size_t n) */
/*	Set a chunk of memory to zero. */
/*	This is a BSD routine that escaped from the kernel.  Don't use. */
/* */
#include <machine/asm.h>

ENTRY(bzero)
	push	%ebp
	movl	%esp, %ebp
	push	12(%ebp)	/* Size */
	push	$0	/* Zero */
	push	8(%ebp)	/* String */
	call	_C_LABEL(memset) /* Call the proper routine */
	leave
	ret
