#include <machine/asm.h>

IMPORT(_read)
ENTRY(read)
	jmp	_C_LABEL(_read)

