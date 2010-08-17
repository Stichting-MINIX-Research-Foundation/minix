#include <machine/asm.h>

IMPORT(_execvp)
ENTRY(execvp)
	jmp	_C_LABEL(_execvp)

