#include <machine/asm.h>

IMPORT(_vm_set_priv)
ENTRY(vm_set_priv)
	jmp	_C_LABEL(_vm_set_priv)

