.sect .text
.extern	__stime
.define	_stime
.align 2

_stime:
	jmp	__stime
