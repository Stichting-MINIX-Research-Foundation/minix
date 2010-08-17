#include <machine/asm.h>

IMPORT(_getnucred)
ENTRY(getnucred)
	jmp	_C_LABEL(_getnucred)

