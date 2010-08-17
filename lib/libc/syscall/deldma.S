#include <machine/asm.h>

IMPORT(_deldma)
ENTRY(deldma)
	jmp	_C_LABEL(_deldma)

