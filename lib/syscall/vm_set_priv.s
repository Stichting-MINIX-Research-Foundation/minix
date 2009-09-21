.sect .text
.extern __vm_set_priv
.define _vm_set_priv
.align 2

_vm_set_priv:
	jmp	__vm_set_priv
