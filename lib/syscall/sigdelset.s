.sect .text
.extern	__sigdelset
.define	_sigdelset
.align 2

_sigdelset:
	jmp	__sigdelset
