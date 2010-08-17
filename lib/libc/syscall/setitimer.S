#include <machine/asm.h>

IMPORT(_setitimer)
ENTRY(setitimer)
	jmp	_C_LABEL(_setitimer)

