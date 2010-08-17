#include <machine/asm.h>

IMPORT(_getdents)
ENTRY(getdents)
	jmp	_C_LABEL(_getdents)

