.globl	__brksize
.data
.extern	endbss, __brksize
__brksize:
.long	endbss
