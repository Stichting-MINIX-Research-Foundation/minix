#include <machine/asm.h>

IMPORT(_ioctl)
ENTRY(ioctl)
	jmp	_C_LABEL(_ioctl)

