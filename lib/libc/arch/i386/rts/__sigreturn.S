/* This routine is the low-level code for returning from signals.   */
/* It calls __sigreturn, which is the normal "system call" routine. */
/* Both ___sigreturn and __sigreturn are needed. */
.text
.globl	___sigreturn
.extern	__sigreturn
___sigreturn:
	addl	$16, %esp
	jmp	__sigreturn
