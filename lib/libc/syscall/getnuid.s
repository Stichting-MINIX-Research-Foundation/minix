.sect .text
.extern	__getnuid
.define	_getnuid
.align 2

_getnuid:
	jmp	__getnuid
