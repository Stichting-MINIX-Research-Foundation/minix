#include <machine/asm.h>

IMPORT(_sprofile)
ENTRY(sprofile)
	jmp	_C_LABEL(_sprofile)

