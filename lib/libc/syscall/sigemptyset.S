#include <machine/asm.h>

IMPORT(_sigemptyset)
ENTRY(sigemptyset)
	jmp	_C_LABEL(_sigemptyset)

