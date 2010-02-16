.sect .text
.extern	__pathconf
.define	_pathconf
.align 2

_pathconf:
	jmp	__pathconf
