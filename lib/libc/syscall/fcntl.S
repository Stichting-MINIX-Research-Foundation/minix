#include <machine/asm.h>

IMPORT(_fcntl)
ENTRY(fcntl)
	jmp	_C_LABEL(_fcntl)

