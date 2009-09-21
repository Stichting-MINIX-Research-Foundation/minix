.sect .text
.extern __vm_query_exit
.define _vm_query_exit
.align 2

_vm_query_exit:
	jmp	__vm_query_exit
