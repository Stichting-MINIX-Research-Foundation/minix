#include <machine/asm.h>

IMPORT(_munmap)
ENTRY(munmap)
	jmp	_C_LABEL(_munmap)

IMPORT(_munmap_text)
ENTRY(munmap_text)
	jmp	_C_LABEL(_munmap_text)

