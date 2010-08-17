#include <machine/asm.h>

IMPORT(_getuid)
ENTRY(getuid)
	jmp	_C_LABEL(_getuid)

