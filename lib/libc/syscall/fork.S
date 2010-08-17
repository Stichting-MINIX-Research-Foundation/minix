#include <machine/asm.h>

IMPORT(_fork)
ENTRY(fork)
	jmp	_C_LABEL(_fork)

