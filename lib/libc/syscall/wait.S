#include <machine/asm.h>

IMPORT(_wait)
ENTRY(wait)
	jmp	_C_LABEL(_wait)

