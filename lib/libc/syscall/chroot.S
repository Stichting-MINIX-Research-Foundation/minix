#include <machine/asm.h>

IMPORT(_chroot)
ENTRY(chroot)
	jmp	_C_LABEL(_chroot)

