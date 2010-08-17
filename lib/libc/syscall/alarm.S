#include <machine/asm.h>

IMPORT(_alarm)
ENTRY(alarm)
	jmp	_C_LABEL(_alarm)

