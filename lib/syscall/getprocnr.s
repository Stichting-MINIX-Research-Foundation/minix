.sect .text
.extern	__getprocnr
.define	_getprocnr
.align 2

_getprocnr:
	jmp	__getprocnr
