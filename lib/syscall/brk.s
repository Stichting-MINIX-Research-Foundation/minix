.sect .text
.extern	__brk
.define	_brk
.align 2

_brk:
	jmp	__brk
