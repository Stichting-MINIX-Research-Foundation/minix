.sect .text; .sect .rom; .sect .data; .sect .bss
.define __send, __receive, __sendrec

! See ../h/com.h for C definitions
SEND = 1
RECEIVE = 2
BOTH = 3
NB_SEND = 1 + 16 		! SEND | 0xF0
NB_RECEIVE = 2 + 16		! RECEIVE | 0xF0
SYSVEC = 33

SRCDEST = 8
MESSAGE = 12

!*========================================================================*
!                           _send and _receive                            *
!*========================================================================*
! _send(), _nb_send(), _receive(), _nb_receive(), and _sendrec() all 
! save ebp, but destroy eax and ecx.
.define __send, __nb_send, __receive, __nb_receive, __sendrec
.sect .text
__send:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRCDEST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SEND		! _send(dest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__nb_send:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRCDEST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, NB_SEND		! _nb_send(dest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__receive:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRCDEST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, RECEIVE		! _receive(src, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__nb_receive:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRCDEST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, NB_RECEIVE		! _nb_receive(src, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__sendrec:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRCDEST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, BOTH		! _sendrec(srcdest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret
