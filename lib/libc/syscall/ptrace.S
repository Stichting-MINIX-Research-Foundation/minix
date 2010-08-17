#include <machine/asm.h>

IMPORT(_ptrace)
ENTRY(ptrace)
	jmp	_C_LABEL(_ptrace)

