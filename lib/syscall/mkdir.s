.sect .text
.extern	__mkdir
.define	_mkdir
.align 2

_mkdir:
	jmp	__mkdir
