.text
.extern	__vm_getrefcount
.globl	_vm_getrefcount
.balign	2

_vm_getrefcount:
	jmp	__vm_getrefcount
