.sect .text
.extern	__sleep
.define	_sleep
.align 2

_sleep:
	jmp	__sleep
