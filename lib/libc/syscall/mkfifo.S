#include <machine/asm.h>

IMPORT(_mkfifo)
ENTRY(mkfifo)
	jmp	_C_LABEL(_mkfifo)

