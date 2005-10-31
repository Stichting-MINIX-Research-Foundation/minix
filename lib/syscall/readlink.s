.sect .text
.extern	__readlink
.define	_readlink
.align 2

_readlink:
	jmp	__readlink
