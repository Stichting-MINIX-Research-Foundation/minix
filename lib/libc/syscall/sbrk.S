#include <machine/asm.h>

IMPORT(_sbrk)
ENTRY(sbrk)
	jmp	_C_LABEL(_sbrk)

