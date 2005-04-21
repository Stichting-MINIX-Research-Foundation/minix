.sect .text
.extern	__pipe
.define	_pipe
.align 2

_pipe:
	jmp	__pipe
