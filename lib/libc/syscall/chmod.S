#include <machine/asm.h>

IMPORT(_chmod)
ENTRY(chmod)
	jmp	_C_LABEL(_chmod)

