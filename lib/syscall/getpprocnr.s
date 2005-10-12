.sect .text
.extern	__getpprocnr
.define	_getpprocnr
.align 2

_getpprocnr:
	jmp	__getpprocnr
