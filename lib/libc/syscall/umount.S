#include <machine/asm.h>

IMPORT(_umount)
ENTRY(umount)
	jmp	_C_LABEL(_umount)

