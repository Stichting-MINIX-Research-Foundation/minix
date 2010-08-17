#include <machine/asm.h>

IMPORT(_utime)
ENTRY(utime)
	jmp	_C_LABEL(_utime)

