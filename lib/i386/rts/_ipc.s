.sect .text; .sect .rom; .sect .data; .sect .bss
.define __echo, __notify, __send, __nb_send, __receive, __nb_receive, __sendrec 

! See src/kernel/ipc.h for C definitions
SEND = 1
RECEIVE = 2
SENDREC = 3 
NOTIFY = 4
ECHO = 8
NB_SEND = 1 + 16 		! flags 0x10 to prevent blocking 
NB_RECEIVE = 2 + 16		! flags 0x10 to prevent blocking 
SYSVEC = 33			! trap to kernel 

SRC_DST = 8			! source/ destination process 
ECHO_MESS = 8			! doesn't have SRC_DST 
MESSAGE = 12			! message pointer 

!*========================================================================*
!                           IPC assembly routines			  *
!*========================================================================*
! all message passing routines save ebp, but destroy eax and ecx.
.define __echo, __notify, __send, __nb_send, __receive, __nb_receive, __sendrec 
.sect .text
__send:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = dest-src
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
	mov	eax, SRC_DST(ebp)	! eax = dest-src
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
	mov	eax, SRC_DST(ebp)	! eax = dest-src
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
	mov	eax, SRC_DST(ebp)	! eax = dest-src
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
	mov	eax, SRC_DST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SENDREC		! _sendrec(srcdest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__notify:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! ebx = destination 
	mov	ecx, NOTIFY		! _notify(srcdst)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__echo:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	ebx, ECHO_MESS(ebp)	! ebx = message pointer
	mov	ecx, ECHO		! _echo(srcdest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

