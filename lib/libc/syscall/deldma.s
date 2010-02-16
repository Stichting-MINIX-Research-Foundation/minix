.sect .text
.extern	__deldma
.define	_deldma
.align 2

_deldma:
	jmp	__deldma
