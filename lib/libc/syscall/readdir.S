.text
.extern	__readdir
.globl	_readdir
.balign	2

_readdir:
	jmp	__readdir
