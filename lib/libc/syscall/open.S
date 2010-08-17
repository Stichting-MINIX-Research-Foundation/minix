#include <machine/asm.h>

IMPORT(_open)
ENTRY(open)
	jmp	_C_LABEL(_open)

