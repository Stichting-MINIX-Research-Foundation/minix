.define .ilar
.text

.ilar:
	pop     cx
	pop     dx
.extern .unknown
	cmp     dx,#2
	jne     .unknown
	pop     bx      ! descriptor address
	pop     ax      ! index
	push    cx
.extern .lar2
	jmp    .lar2
