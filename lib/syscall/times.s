.sect .text
.extern	__times
.define	_times
.align 2

_times:
	jmp	__times
