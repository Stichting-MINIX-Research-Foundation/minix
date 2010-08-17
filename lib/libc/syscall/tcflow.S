#include <machine/asm.h>

IMPORT(_tcflow)
ENTRY(tcflow)
	jmp	_C_LABEL(_tcflow)

