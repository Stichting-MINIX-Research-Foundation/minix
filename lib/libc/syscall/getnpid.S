#include <machine/asm.h>

IMPORT(_getnpid)
ENTRY(getnpid)
	jmp	_C_LABEL(_getnpid)

