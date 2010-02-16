.sect .text
.extern __setitimer
.define _setitimer
.align 2

_setitimer:
	jmp	__setitimer
