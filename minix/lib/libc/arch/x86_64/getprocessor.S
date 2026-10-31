/*	getprocessor() - determine processor type (x86_64)	*/
/*
 * On x86_64 CPUID is always available and the processor is always ≥586,
 * so the 386/486 AC-bit/ID-bit probing is unnecessary.
 */
#include <machine/asm.h>

/* int getprocessor(void); */
/* Returns 586, 686, ... */
ENTRY(getprocessor)
	pushq	%rbx			/* cpuid clobbers %rbx (callee-saved) */

	movl	$1, %eax
	cpuid				/* EAX[11:8] = base family */

	shrl	$8, %eax		/* family field → bits [3:0] */
	andl	$0x0F, %eax

	cmpl	$15, %eax		/* 15 = extended family; normalise to 6 */
	jne	.Ldirect
	movl	$6, %eax
.Ldirect:
	imull	$100, %eax		/* 600, ... */
	addl	$86, %eax		/* 686, ... */

	popq	%rbx
	ret
