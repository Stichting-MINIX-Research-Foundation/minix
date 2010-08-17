#include <machine/asm.h>

IMPORT(_nanosleep)
ENTRY(nanosleep)
	jmp	_C_LABEL(_nanosleep)

