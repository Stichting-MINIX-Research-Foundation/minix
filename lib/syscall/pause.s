.sect .text
.extern	__pause
.define	_pause
.align 2

_pause:
	jmp	__pause
