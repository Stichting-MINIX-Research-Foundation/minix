#include <machine/asm.h>

IMPORT(_brk)
ENTRY(brk)
	jmp	_C_LABEL(_brk)

