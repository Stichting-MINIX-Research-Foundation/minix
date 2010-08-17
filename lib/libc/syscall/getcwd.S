#include <machine/asm.h>

IMPORT(_getcwd)
ENTRY(getcwd)
	jmp	_C_LABEL(_getcwd)

