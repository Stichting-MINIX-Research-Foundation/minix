#include <machine/asm.h>

IMPORT(_setuid)
ENTRY(setuid)
	jmp	_C_LABEL(_setuid)

IMPORT(_seteuid)
ENTRY(seteuid)
	jmp	_C_LABEL(_seteuid)

