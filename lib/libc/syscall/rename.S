#include <machine/asm.h>

IMPORT(_rename)
ENTRY(rename)
	jmp	_C_LABEL(_rename)

