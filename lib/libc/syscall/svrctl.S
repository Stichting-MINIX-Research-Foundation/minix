#include <machine/asm.h>

IMPORT(_svrctl)
ENTRY(svrctl)
	jmp	_C_LABEL(_svrctl)

