#include <machine/asm.h>

IMPORT(_uname)
ENTRY(uname)
	jmp	_C_LABEL(_uname)

