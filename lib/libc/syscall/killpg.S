#include <machine/asm.h>

IMPORT(_killpg)
ENTRY(killpg)
	jmp	_C_LABEL(_killpg)

