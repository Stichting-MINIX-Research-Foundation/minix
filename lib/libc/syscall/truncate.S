#include <machine/asm.h>

IMPORT(_truncate)
ENTRY(truncate)
	jmp	_C_LABEL(_truncate)

IMPORT(_ftruncate)
ENTRY(ftruncate)
	jmp	_C_LABEL(_ftruncate)

