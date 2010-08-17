#include <machine/asm.h>

IMPORT(_sigpending)
ENTRY(sigpending)
	jmp	_C_LABEL(_sigpending)

