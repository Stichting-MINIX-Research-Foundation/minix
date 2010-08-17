#include <machine/asm.h>

IMPORT(_readlink)
ENTRY(readlink)
	jmp	_C_LABEL(_readlink)

