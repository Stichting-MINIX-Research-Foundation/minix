.sect .text
.extern	__cprofile
.define	_cprofile
.align 2

_cprofile:
	jmp	__cprofile
