#include <machine/asm.h>

IMPORT(_sigdelset)
ENTRY(sigdelset)
	jmp	_C_LABEL(_sigdelset)

