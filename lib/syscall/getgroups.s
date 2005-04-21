.sect .text
.extern	__getgroups
.define	_getgroups
.align 2

_getgroups:
	jmp	__getgroups
