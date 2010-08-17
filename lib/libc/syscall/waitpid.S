#include <machine/asm.h>

IMPORT(_waitpid)
ENTRY(waitpid)
	jmp	_C_LABEL(_waitpid)

