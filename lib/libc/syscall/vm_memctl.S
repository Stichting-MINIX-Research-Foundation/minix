.text
.extern	__vm_memctl
.globl	_vm_memctl
.balign	2

_vm_memctl:
	jmp	__vm_memctl
