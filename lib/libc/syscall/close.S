#include <machine/asm.h>

IMPORT(_close)
ENTRY(close)
	jmp	_C_LABEL(_close)

