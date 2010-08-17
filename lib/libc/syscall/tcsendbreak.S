#include <machine/asm.h>

IMPORT(_tcsendbreak)
ENTRY(tcsendbreak)
	jmp	_C_LABEL(_tcsendbreak)

