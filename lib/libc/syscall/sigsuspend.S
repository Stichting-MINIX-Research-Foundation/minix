#include <machine/asm.h>

IMPORT(_sigsuspend)
ENTRY(sigsuspend)
	jmp	_C_LABEL(_sigsuspend)

