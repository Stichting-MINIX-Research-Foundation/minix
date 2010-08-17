#include <machine/asm.h>

IMPORT(_sigaddset)
ENTRY(sigaddset)
	jmp	_C_LABEL(_sigaddset)

