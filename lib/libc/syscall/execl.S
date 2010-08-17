#include <machine/asm.h>

IMPORT(_execl)
ENTRY(execl)
	jmp	_C_LABEL(_execl)

