#include <machine/asm.h>

IMPORT(_sigismember)
ENTRY(sigismember)
	jmp	_C_LABEL(_sigismember)

