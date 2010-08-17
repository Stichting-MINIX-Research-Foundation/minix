#include <machine/asm.h>

IMPORT(_sigaction)
ENTRY(sigaction)
	jmp	_C_LABEL(_sigaction)

