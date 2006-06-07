.sect .text
.extern	__killpg
.define	_killpg
.align 2

_killpg:
	jmp	__killpg
