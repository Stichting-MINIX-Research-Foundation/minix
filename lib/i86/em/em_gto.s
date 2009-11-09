.define .gto
.text

.gto:
	mov     bp,4(bx)
	mov     sp,2(bx)
	jmp     @(bx)
