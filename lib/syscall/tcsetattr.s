.sect .text
.extern	__tcsetattr
.define	_tcsetattr
.align 2

_tcsetattr:
	jmp	__tcsetattr
