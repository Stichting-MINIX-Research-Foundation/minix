.sect .text
.extern	__getnprocnr
.define	_getnprocnr
.align 2

_getnprocnr:
	jmp	__getnprocnr
