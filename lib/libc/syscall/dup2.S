#include <machine/asm.h>

IMPORT(_dup2)
ENTRY(dup2)
	jmp	_C_LABEL(_dup2)

