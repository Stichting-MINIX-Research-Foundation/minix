.sect .text
.extern	__findproc
.define	_findproc
.align 2

_findproc:
	jmp	__findproc
