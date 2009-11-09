.sect .text
.extern	__ptrace
.define	_ptrace
.align 2

_ptrace:
	jmp	__ptrace
