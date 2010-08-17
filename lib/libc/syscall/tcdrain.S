#include <machine/asm.h>

IMPORT(_tcdrain)
ENTRY(tcdrain)
	jmp	_C_LABEL(_tcdrain)

