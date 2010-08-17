#include <machine/asm.h>

IMPORT(_pathconf)
ENTRY(pathconf)
	jmp	_C_LABEL(_pathconf)

