#include <machine/asm.h>

IMPORT(_mknod)
ENTRY(mknod)
	jmp	_C_LABEL(_mknod)

