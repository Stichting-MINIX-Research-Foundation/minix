#include <machine/asm.h>

IMPORT(_getppid)
ENTRY(getppid)
	jmp	_C_LABEL(_getppid)

