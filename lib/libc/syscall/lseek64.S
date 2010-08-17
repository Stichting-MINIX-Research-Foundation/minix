#include <machine/asm.h>

IMPORT(_lseek64)
ENTRY(lseek64)
	jmp	_C_LABEL(_lseek64)

