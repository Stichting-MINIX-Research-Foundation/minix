#include <machine/asm.h>

IMPORT(_unlink)
ENTRY(unlink)
	jmp	_C_LABEL(_unlink)

