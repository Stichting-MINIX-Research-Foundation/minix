.text
.extern	__vm_adddma
.globl	_vm_adddma
.extern	__vm_deldma
.globl	_vm_deldma
.extern	__vm_getdma
.globl	_vm_getdma
.balign	2

_vm_adddma:
	jmp	__vm_adddma
_vm_deldma:
	jmp	__vm_deldma
_vm_getdma:
	jmp	__vm_getdma
