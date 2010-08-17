#include <machine/asm.h>

IMPORT(_access)
ENTRY(access)
	jmp	_C_LABEL(_access)

