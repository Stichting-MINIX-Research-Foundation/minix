#include <machine/asm.h>

IMPORT(_opendir)
ENTRY(opendir)
	jmp	_C_LABEL(_opendir)

