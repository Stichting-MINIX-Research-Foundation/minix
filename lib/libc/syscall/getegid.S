#include <machine/asm.h>

IMPORT(_getegid)
ENTRY(getegid)
	jmp	_C_LABEL(_getegid)

