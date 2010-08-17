#include <machine/asm.h>

IMPORT(_getsysinfo)
ENTRY(getsysinfo)
	jmp	_C_LABEL(_getsysinfo)

IMPORT(_getsysinfo_up)
ENTRY(getsysinfo_up)
	jmp	_C_LABEL(_getsysinfo_up)

