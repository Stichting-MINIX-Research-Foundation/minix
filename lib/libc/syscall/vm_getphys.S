#include <machine/asm.h>

IMPORT(_vm_getphys)
ENTRY(vm_getphys)
	jmp	_C_LABEL(_vm_getphys)

