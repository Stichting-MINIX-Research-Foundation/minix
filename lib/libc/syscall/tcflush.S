#include <machine/asm.h>

IMPORT(_tcflush)
ENTRY(tcflush)
	jmp	_C_LABEL(_tcflush)

