.sect .text
.extern	__getsigset
.define	_getsigset
.align 2

_getsigset:
	jmp	__getsigset
