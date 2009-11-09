.define .ret8
.text
.extern .retarea

.ret8:
	pop	bx
	pop	.retarea
	pop	.retarea+2
	pop	.retarea+4
	pop	.retarea+6
	jmp	(bx)
