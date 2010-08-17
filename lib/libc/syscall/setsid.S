#include <machine/asm.h>

IMPORT(_setsid)
ENTRY(setsid)
	jmp	_C_LABEL(_setsid)

