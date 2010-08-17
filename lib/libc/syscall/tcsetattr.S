#include <machine/asm.h>

IMPORT(_tcsetattr)
ENTRY(tcsetattr)
	jmp	_C_LABEL(_tcsetattr)

