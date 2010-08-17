#include <machine/asm.h>

IMPORT(_getpprocnr)
ENTRY(getpprocnr)
	jmp	_C_LABEL(_getpprocnr)

