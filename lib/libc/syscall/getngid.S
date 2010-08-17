#include <machine/asm.h>

IMPORT(_getngid)
ENTRY(getngid)
	jmp	_C_LABEL(_getngid)

