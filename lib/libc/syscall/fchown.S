#include <machine/asm.h>

IMPORT(_fchown)
ENTRY(fchown)
	jmp	_C_LABEL(_fchown)

