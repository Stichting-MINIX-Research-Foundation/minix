#include <machine/asm.h>

IMPORT(_kill)
ENTRY(kill)
	jmp	_C_LABEL(_kill)

