#include <machine/asm.h>

IMPORT(_getsigset)
ENTRY(getsigset)
	jmp	_C_LABEL(_getsigset)

