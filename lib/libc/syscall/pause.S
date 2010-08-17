#include <machine/asm.h>

IMPORT(_pause)
ENTRY(pause)
	jmp	_C_LABEL(_pause)

