.define .isar
.text

.isar:
	pop     cx
	pop     ax
	cmp     ax,#2
.extern .unknown
	jne     .unknown
	pop     bx      ! descriptor address
	pop     ax      ! index
	push    cx
.extern .sar2
	jmp    .sar2
