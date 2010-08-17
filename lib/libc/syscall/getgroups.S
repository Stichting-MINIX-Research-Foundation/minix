#include <machine/asm.h>

IMPORT(_getgroups)
ENTRY(getgroups)
	jmp	_C_LABEL(_getgroups)

