#include <machine/asm.h>

IMPORT(_dup)
ENTRY(dup)
	jmp	_C_LABEL(_dup)

