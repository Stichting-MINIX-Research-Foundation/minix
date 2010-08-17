#include <machine/asm.h>

IMPORT(_fpathconf)
ENTRY(fpathconf)
	jmp	_C_LABEL(_fpathconf)

