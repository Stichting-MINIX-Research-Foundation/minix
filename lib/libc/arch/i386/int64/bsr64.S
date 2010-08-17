/*	bsr64() - 64 bit bit scan reverse	Author: Erik van der Kouwe */
/*							       15 May 2010 */
#include <minix/compiler.h>
#include <machine/asm.h>

ENTRY(bsr64)
/* int bsr64(u64_t i); */
	bsr	8(%esp),	%eax	/* check high-order DWORD */
	jnz	0f			/* non-zero: return index+32 */
	bsr	4(%esp),	%eax	/* check low-order DWORD */
	jnz	1f			/* non-zero: return index */
	movl	$-1,		%eax	/* both were zero, return -1 */
	jmp	1f
0:	addl	$32,		%eax	/* add 32 to high-order index */
1:	ret
