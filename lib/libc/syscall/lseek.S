#include <machine/asm.h>

IMPORT(_lseek)
ENTRY(lseek)
	jmp	_C_LABEL(_lseek)

