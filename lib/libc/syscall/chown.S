#include <machine/asm.h>

IMPORT(_chown)
ENTRY(chown)
	jmp	_C_LABEL(_chown)

