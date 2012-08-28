/* This routine is the low-level code for returning from signals.   */
/* It calls _sigreturn, which is the normal "system call" routine. */
/* Both __sigreturn and _sigreturn are needed. */
#include <machine/asm.h>

IMPORT(_sigreturn)
ENTRY(__sigreturn)
	add	sp, sp, #16
	b	_C_LABEL(_sigreturn)
