.sect .text
.extern	__fstatfs
.define	_fstatfs
.align 2

_fstatfs:
	jmp	__fstatfs
