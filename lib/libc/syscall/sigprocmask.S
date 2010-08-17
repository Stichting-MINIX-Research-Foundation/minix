#include <machine/asm.h>

IMPORT(_sigprocmask)
ENTRY(sigprocmask)
	jmp	_C_LABEL(_sigprocmask)

