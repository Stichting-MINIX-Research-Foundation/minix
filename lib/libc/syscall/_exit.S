#include <machine/asm.h>

IMPORT(__exit)
ENTRY(_exit)
	jmp	_C_LABEL(__exit)

