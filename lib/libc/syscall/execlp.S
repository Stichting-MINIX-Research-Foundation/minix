#include <machine/asm.h>

IMPORT(_execlp)
ENTRY(execlp)
	jmp	_C_LABEL(_execlp)

