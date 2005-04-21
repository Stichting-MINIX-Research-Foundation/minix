.sect .text
.extern	__tcdrain
.define	_tcdrain
.align 2

_tcdrain:
	jmp	__tcdrain
