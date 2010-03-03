.text
.extern	__ptrace
.globl	_ptrace
.balign	2

_ptrace:
	jmp	__ptrace
