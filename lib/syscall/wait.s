.sect .text
.extern	__wait
.define	_wait
.align 2

_wait:
	jmp	__wait
