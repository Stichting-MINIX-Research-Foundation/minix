.sect .text
.extern	__sbrk
.define	_sbrk
.align 2

_sbrk:
	jmp	__sbrk
