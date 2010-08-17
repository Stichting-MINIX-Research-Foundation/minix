#include <machine/asm.h>

IMPORT(_vm_remap)
ENTRY(vm_remap)
	jmp	_C_LABEL(_vm_remap)

