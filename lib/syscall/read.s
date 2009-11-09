.sect .text
.extern	__read
.define	_read
.align 2

_read:
	jmp	__read
