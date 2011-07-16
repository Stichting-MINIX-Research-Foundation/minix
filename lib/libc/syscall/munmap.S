#include <machine/asm.h>

IMPORT(_minix_munmap)
ENTRY(minix_munmap)
	jmp	_C_LABEL(_minix_munmap)

IMPORT(_minix_munmap_text)
ENTRY(minix_munmap_text)
	jmp	_C_LABEL(_minix_munmap_text)

