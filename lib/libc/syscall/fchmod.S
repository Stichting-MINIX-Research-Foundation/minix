#include <machine/asm.h>

IMPORT(_fchmod)
ENTRY(fchmod)
	jmp	_C_LABEL(_fchmod)

