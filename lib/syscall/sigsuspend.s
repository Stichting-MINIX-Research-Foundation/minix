.sect .text
.extern	__sigsuspend
.define	_sigsuspend
.align 2

_sigsuspend:
	jmp	__sigsuspend
