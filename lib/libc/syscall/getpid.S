#include <machine/asm.h>

IMPORT(_getpid)
ENTRY(getpid)
	jmp	_C_LABEL(_getpid)

