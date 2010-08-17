#include <machine/asm.h>

IMPORT(_cprofile)
ENTRY(cprofile)
	jmp	_C_LABEL(_cprofile)

