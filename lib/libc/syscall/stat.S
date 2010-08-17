#include <machine/asm.h>

IMPORT(_stat)
ENTRY(stat)
	jmp	_C_LABEL(_stat)

