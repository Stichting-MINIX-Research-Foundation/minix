#include <machine/asm.h>

IMPORT(_stime)
ENTRY(stime)
	jmp	_C_LABEL(_stime)

