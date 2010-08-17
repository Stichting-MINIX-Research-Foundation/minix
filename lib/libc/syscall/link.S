#include <machine/asm.h>

IMPORT(_link)
ENTRY(link)
	jmp	_C_LABEL(_link)

