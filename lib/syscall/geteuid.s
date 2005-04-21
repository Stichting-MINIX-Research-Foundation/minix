.sect .text
.extern	__geteuid
.define	_geteuid
.align 2

_geteuid:
	jmp	__geteuid
