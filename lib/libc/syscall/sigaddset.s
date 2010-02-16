.sect .text
.extern	__sigaddset
.define	_sigaddset
.align 2

_sigaddset:
	jmp	__sigaddset
