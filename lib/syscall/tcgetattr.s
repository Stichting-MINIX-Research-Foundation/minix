.sect .text
.extern	__tcgetattr
.define	_tcgetattr
.align 2

_tcgetattr:
	jmp	__tcgetattr
