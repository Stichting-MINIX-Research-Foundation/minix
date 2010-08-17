#include <machine/asm.h>

IMPORT(_reboot)
ENTRY(reboot)
	jmp	_C_LABEL(_reboot)

