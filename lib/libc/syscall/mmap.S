#include <machine/asm.h>

IMPORT(_minix_mmap)
ENTRY(minix_mmap)
	jmp	_C_LABEL(_minix_mmap)

