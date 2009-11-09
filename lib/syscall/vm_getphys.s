.sect .text
.extern	__vm_getphys
.define	_vm_getphys
.align 2

_vm_getphys:
	jmp	__vm_getphys
