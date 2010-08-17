#include <machine/asm.h>

IMPORT(_statvfs)
ENTRY(statvfs)
	jmp	_C_LABEL(_statvfs)

