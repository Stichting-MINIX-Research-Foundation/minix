#include <machine/asm.h>

IMPORT(_select)
ENTRY(select)
	jmp	_C_LABEL(_select)

