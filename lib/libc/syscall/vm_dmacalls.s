.sect .text
.extern	__vm_adddma
.define	_vm_adddma
.extern	__vm_deldma
.define	_vm_deldma
.extern	__vm_getdma
.define	_vm_getdma
.align 2

_vm_adddma:
	jmp	__vm_adddma
_vm_deldma:
	jmp	__vm_deldma
_vm_getdma:
	jmp	__vm_getdma

