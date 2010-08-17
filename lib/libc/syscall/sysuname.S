#include <machine/asm.h>

IMPORT(_sysuname)
ENTRY(sysuname)
	jmp	_C_LABEL(_sysuname)

