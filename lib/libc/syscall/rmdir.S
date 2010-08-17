#include <machine/asm.h>

IMPORT(_rmdir)
ENTRY(rmdir)
	jmp	_C_LABEL(_rmdir)

