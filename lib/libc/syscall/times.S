#include <machine/asm.h>

IMPORT(_times)
ENTRY(times)
	jmp	_C_LABEL(_times)

