.text
.extern	__cprofile
.globl	_cprofile
.balign	2

_cprofile:
	jmp	__cprofile
