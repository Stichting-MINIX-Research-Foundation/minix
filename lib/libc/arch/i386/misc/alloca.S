/* */
/*	alloca() - allocate space on the stack		Author: Kees J. Bot */
/*								2 Dec 1993 */

#include <machine/asm.h>

ENTRY(alloca)
#if __ACK__
	pop	%ecx	/* Return address */
	pop	%eax	/* Bytes to allocate */
	addl	$2*4+3, %eax	/* Add space for two saved register variables */
	andb	$0xF8, %al	/* Align */
	movl	%esp, %ebx	/* Keep current esp */
	subl	%eax, %esp	/* Lower stack */
	movl	%esp, %eax	/* Return value */
	push	4(%ebx)	/* Push what is probably the saved esi */
	push	(%ebx)	/* Saved edi */
/* Now ACK can still do: */
/*	pop edi; pop esi; leave; ret */
	push	%eax	/* Dummy argument */
	jmp	*%ecx
#else
	pop	%ecx	/* Return address */
	pop	%eax	/* Bytes to allocate */
	addl	$3, %eax
	andb	$0xF8, %al	/* Align */
	subl	%eax, %esp	/* Lower stack */
	movl	%esp, %eax	/* Return value */
	push	%eax	/* Dummy argument */
	jmp	*%ecx
#endif
