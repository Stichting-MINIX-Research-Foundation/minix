/* This routine is the low-level code for returning from signals.   */
/* It calls _sigreturn, which is the normal "system call" routine. */
/* Both __sigreturn and _sigreturn are needed. */
#include <machine/asm.h>

IMPORT(_sigreturn)
ENTRY(__sigreturn)
	addl	$16, %esp
#ifndef PIC
	jmp	_C_LABEL(_sigreturn)
#else
	PIC_PROLOGUE		/* push %ebx, but we do not care */
	pop	%eax /* special knowledge of how PIC works: discard pushed EBX */
	jmp	PIC_PLT(_C_LABEL(_sigreturn))
#endif /* PIC */
