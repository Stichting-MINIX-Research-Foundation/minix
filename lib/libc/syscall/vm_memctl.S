#include <machine/asm.h>

IMPORT(_vm_memctl)
ENTRY(vm_memctl)
	jmp	_C_LABEL(_vm_memctl)

