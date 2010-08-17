#include <machine/asm.h>

IMPORT(_vm_query_exit)
ENTRY(vm_query_exit)
	jmp	_C_LABEL(_vm_query_exit)

