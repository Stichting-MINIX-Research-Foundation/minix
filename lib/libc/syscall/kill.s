.sect .text
.extern	__kill
.define	_kill
.align 2

_kill:
	jmp	__kill
