.sect .text
.extern	__rename
.define	_rename
.align 2

_rename:
	jmp	__rename
