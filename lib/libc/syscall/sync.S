#include <machine/asm.h>

IMPORT(_sync)
ENTRY(sync)
	jmp	_C_LABEL(_sync)

