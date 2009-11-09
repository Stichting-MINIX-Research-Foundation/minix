.define __send, __receive, __sendrec

! See ../h/com.h for C definitions
SEND = 1
RECEIVE = 2
BOTH = 3
SYSVEC = 32

!*========================================================================*
!                           _send and _receive                            *
!*========================================================================*
! _send(), _receive(), _sendrec() all save bp, but destroy ax, bx, and cx.
.extern __send, __receive, __sendrec
__send:	mov cx,*SEND		! _send(dest, ptr)
	jmp L0

__receive:
	mov cx,*RECEIVE		! _receive(src, ptr)
	jmp L0

__sendrec:
	mov cx,*BOTH		! _sendrec(srcdest, ptr)
	jmp L0

  L0:	push bp			! save bp
	mov bp,sp		! can't index off sp
	mov ax,4(bp)		! ax = dest-src
	mov bx,6(bp)		! bx = message pointer
	int SYSVEC		! trap to the kernel
	pop bp			! restore bp
	ret			! return

