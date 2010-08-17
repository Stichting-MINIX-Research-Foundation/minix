#include <machine/asm.h>

IMPORT(_umask)
ENTRY(umask)
	jmp	_C_LABEL(_umask)

