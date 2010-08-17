#include <machine/asm.h>

IMPORT(_vm_getrefcount)
ENTRY(vm_getrefcount)
	jmp	_C_LABEL(_vm_getrefcount)

