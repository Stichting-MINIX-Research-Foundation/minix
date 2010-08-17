#include <machine/asm.h>

IMPORT(_mmap)
ENTRY(mmap)
	jmp	_C_LABEL(_mmap)

