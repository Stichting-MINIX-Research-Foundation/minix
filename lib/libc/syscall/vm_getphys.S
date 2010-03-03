.text
.extern	__vm_getphys
.globl	_vm_getphys
.balign	2

_vm_getphys:
	jmp	__vm_getphys
