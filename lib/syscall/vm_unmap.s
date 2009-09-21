.sect .text
.extern	__vm_unmap
.define	_vm_unmap
.align 2

_vm_unmap:
	jmp	__vm_unmap
