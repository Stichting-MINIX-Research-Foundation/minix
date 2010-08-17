#include <machine/asm.h>

IMPORT(_isatty)
ENTRY(isatty)
	jmp	_C_LABEL(_isatty)

