#include <machine/asm.h>

IMPORT(_getnprocnr)
ENTRY(getnprocnr)
	jmp	_C_LABEL(_getnprocnr)

