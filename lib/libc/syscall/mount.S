#include <machine/asm.h>

IMPORT(_mount)
ENTRY(mount)
	jmp	_C_LABEL(_mount)

