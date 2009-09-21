.sect .text
.extern	__vm_remap
.define	_vm_remap
.align 2

_vm_remap:
	jmp	__vm_remap
