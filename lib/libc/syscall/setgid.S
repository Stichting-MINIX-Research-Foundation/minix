#include <machine/asm.h>

IMPORT(_setgid)
ENTRY(setgid)
	jmp	_C_LABEL(_setgid)

IMPORT(_setegid)
ENTRY(setegid)
	jmp	_C_LABEL(_setegid)

