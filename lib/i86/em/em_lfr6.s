.define .lfr6
.text
.extern .retarea

.lfr6:
	pop	bx
	push	.retarea+4
	push	.retarea+2
	push	.retarea
	jmp	(bx)
