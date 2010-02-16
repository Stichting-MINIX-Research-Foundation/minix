.sect .text
.extern	__stat
.define	_stat
.align 2

_stat:
	jmp	__stat
