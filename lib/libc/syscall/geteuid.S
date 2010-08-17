#include <machine/asm.h>

IMPORT(_geteuid)
ENTRY(geteuid)
	jmp	_C_LABEL(_geteuid)

