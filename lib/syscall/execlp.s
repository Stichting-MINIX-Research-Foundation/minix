.sect .text
.extern	__execlp
.define	_execlp
.align 2

_execlp:
	jmp	__execlp
