.sect .text
.extern	__close
.define	_close
.align 2

_close:
	jmp	__close
