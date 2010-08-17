#include <machine/asm.h>

IMPORT(_execle)
ENTRY(execle)
	jmp	_C_LABEL(_execle)

