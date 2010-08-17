#include <machine/asm.h>

IMPORT(_fstat)
ENTRY(fstat)
	jmp	_C_LABEL(_fstat)

