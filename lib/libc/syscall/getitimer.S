#include <machine/asm.h>

IMPORT(_getitimer)
ENTRY(getitimer)
	jmp	_C_LABEL(_getitimer)

