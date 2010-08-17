#include <machine/asm.h>

IMPORT(_fstatfs)
ENTRY(fstatfs)
	jmp	_C_LABEL(_fstatfs)

