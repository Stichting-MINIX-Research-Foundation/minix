.sect .text
.extern	__getnpid
.define	_getnpid
.align 2

_getnpid:
	jmp	__getnpid
