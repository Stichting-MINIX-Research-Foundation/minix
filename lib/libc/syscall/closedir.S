#include <machine/asm.h>

IMPORT(_closedir)
ENTRY(closedir)
	jmp	_C_LABEL(_closedir)

