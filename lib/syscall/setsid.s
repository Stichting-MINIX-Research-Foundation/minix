.sect .text
.extern	__setsid
.define	_setsid
.align 2

_setsid:
	jmp	__setsid
