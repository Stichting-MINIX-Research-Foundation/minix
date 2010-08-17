#include <machine/asm.h>

IMPORT(_sigreturn)
ENTRY(sigreturn)
	jmp	_C_LABEL(_sigreturn)

