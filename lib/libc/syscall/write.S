#include <machine/asm.h>

IMPORT(_write)
ENTRY(write)
	jmp	_C_LABEL(_write)

