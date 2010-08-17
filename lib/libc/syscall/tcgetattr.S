#include <machine/asm.h>

IMPORT(_tcgetattr)
ENTRY(tcgetattr)
	jmp	_C_LABEL(_tcgetattr)

