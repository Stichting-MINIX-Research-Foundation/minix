#include <machine/asm.h>

IMPORT(_vm_update)
ENTRY(vm_update)
	jmp	_C_LABEL(_vm_update)

