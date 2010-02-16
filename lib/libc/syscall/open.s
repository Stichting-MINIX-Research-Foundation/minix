.sect .text
.extern	__open
.define	_open
.align 2

_open:
	jmp	__open
