.sect .text
.extern __getitimer
.define _getitimer
.align 2

_getitimer:
	jmp	__getitimer
