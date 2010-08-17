#include <machine/asm.h>

IMPORT(_lstat)
ENTRY(lstat)
	jmp	_C_LABEL(_lstat)

