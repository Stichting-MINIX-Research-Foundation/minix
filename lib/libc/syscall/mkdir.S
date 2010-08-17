#include <machine/asm.h>

IMPORT(_mkdir)
ENTRY(mkdir)
	jmp	_C_LABEL(_mkdir)

