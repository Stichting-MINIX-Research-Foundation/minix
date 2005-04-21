.sect .text
.extern	__sigfillset
.define	_sigfillset
.align 2

_sigfillset:
	jmp	__sigfillset
