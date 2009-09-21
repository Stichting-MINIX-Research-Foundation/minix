.sect .text
.extern	__vm_getrefcount
.define	_vm_getrefcount
.align 2

_vm_getrefcount:
	jmp	__vm_getrefcount
