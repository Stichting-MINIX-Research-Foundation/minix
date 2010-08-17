#include <machine/asm.h>

IMPORT(_execve)
ENTRY(execve)
	jmp	_C_LABEL(_execve)

