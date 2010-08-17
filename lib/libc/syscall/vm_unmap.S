#include <machine/asm.h>

IMPORT(_vm_unmap)
ENTRY(vm_unmap)
	jmp	_C_LABEL(_vm_unmap)

