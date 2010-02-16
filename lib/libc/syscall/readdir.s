.sect .text
.extern	__readdir
.define	_readdir
.align 2

_readdir:
	jmp	__readdir
