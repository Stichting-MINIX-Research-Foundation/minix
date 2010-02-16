.sect .text
.extern	__svrctl
.define	_svrctl
.align 2

_svrctl:
	jmp	__svrctl
