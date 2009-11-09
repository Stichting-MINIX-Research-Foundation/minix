.define .rck
.text

	! descriptor address in bx
	! value in ax, must be left there
.rck:
	cmp     ax,(bx)
	jl      2f
	cmp     ax,2(bx)
	jg      2f
	ret
2:
	push    ax
ERANGE = 1
.extern .error
	mov     ax,#ERANGE
	call    .error
	pop     ax
	ret
