#include <machine/asm.h>

IMPORT(_fstatvfs)
ENTRY(fstatvfs)
	jmp	_C_LABEL(_fstatvfs)

