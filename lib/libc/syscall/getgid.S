#include <machine/asm.h>

IMPORT(_getgid)
ENTRY(getgid)
	jmp	_C_LABEL(_getgid)

