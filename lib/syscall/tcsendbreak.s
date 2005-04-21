.sect .text
.extern	__tcsendbreak
.define	_tcsendbreak
.align 2

_tcsendbreak:
	jmp	__tcsendbreak
