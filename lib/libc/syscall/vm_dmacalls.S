#include <machine/asm.h>

IMPORT(_vm_adddma)
ENTRY(vm_adddma)
	jmp	_C_LABEL(_vm_adddma)

IMPORT(_vm_deldma)
ENTRY(vm_deldma)
	jmp	_C_LABEL(_vm_deldma)

IMPORT(_vm_getdma)
ENTRY(vm_getdma)
	jmp	_C_LABEL(_vm_getdma)

