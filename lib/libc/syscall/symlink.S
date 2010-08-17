#include <machine/asm.h>

IMPORT(_symlink)
ENTRY(symlink)
	jmp	_C_LABEL(_symlink)

