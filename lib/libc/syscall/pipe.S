#include <machine/asm.h>

IMPORT(_pipe)
ENTRY(pipe)
	jmp	_C_LABEL(_pipe)

