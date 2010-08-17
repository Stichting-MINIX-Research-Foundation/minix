#include <machine/asm.h>

IMPORT(_getpgrp)
ENTRY(getpgrp)
	jmp	_C_LABEL(_getpgrp)

