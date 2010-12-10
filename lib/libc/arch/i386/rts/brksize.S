#if defined(__ELF__)
.globl _end
.globl _brksize

.data
_brksize: .long _end
#else
.globl	__brksize
.data
.extern	endbss, __brksize
__brksize:
.long	endbss
#endif
