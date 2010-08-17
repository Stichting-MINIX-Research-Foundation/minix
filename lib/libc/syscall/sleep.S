#include <machine/asm.h>

IMPORT(_sleep)
ENTRY(sleep)
	jmp	_C_LABEL(_sleep)

