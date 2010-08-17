#include <machine/asm.h>

IMPORT(_getdma)
ENTRY(getdma)
	jmp	_C_LABEL(_getdma)

