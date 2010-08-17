#include <machine/asm.h>

IMPORT(_execv)
ENTRY(execv)
	jmp	_C_LABEL(_execv)

