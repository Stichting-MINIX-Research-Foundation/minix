#include <machine/asm.h>

IMPORT(_chdir)
ENTRY(chdir)
	jmp	_C_LABEL(_chdir)

IMPORT(_fchdir)
ENTRY(fchdir)
	jmp	_C_LABEL(_fchdir)

