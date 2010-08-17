#include <machine/asm.h>

IMPORT(_getnuid)
ENTRY(getnuid)
	jmp	_C_LABEL(_getnuid)

