#include <machine/asm.h>

IMPORT(_time)
ENTRY(time)
	jmp	_C_LABEL(_time)

