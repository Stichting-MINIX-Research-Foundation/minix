#include <machine/asm.h>

IMPORT(_rewinddir)
ENTRY(rewinddir)
	jmp	_C_LABEL(_rewinddir)

