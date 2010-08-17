#include <machine/asm.h>

IMPORT(_getprocnr)
ENTRY(getprocnr)
	jmp	_C_LABEL(_getprocnr)

