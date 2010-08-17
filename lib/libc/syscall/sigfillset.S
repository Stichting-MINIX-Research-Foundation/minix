#include <machine/asm.h>

IMPORT(_sigfillset)
ENTRY(sigfillset)
	jmp	_C_LABEL(_sigfillset)

