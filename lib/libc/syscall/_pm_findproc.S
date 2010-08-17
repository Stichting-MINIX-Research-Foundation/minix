#include <machine/asm.h>

IMPORT(__pm_findproc)
ENTRY(_pm_findproc)
	jmp	_C_LABEL(__pm_findproc)

