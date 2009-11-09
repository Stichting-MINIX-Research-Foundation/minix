.sect .text
.extern	__mount
.define	_mount
.align 2

_mount:
	jmp	__mount
