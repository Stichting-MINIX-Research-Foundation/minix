#include <machine/asm.h>

IMPORT(_setgroups)
ENTRY(setgroups)
	jmp	_C_LABEL(_setgroups)

