/* This routine is the low-level code for returning from signals.   */
/* It calls _sigreturn, which is the normal "system call" routine. */
/* Both __sigreturn and _sigreturn are needed. */
#include <machine/asm.h>

IMPORT(_sigreturn)
ENTRY(__sigreturn)
	addl	$16, %esp
	jmp	_C_LABEL(_sigreturn)
