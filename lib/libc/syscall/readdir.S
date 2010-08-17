#include <machine/asm.h>

IMPORT(_readdir)
ENTRY(readdir)
	jmp	_C_LABEL(_readdir)

