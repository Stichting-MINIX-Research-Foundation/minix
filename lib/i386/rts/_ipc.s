.sect .text; .sect .rom; .sect .data; .sect .bss
.define __notify, __send, __senda, __sendnb, __receive, __sendrec 

! See src/kernel/ipc.h for C definitions
SEND = 1
RECEIVE = 2
SENDREC = 3 
NOTIFY = 4
SENDNB = 5
SENDA = 16
SYSVEC = 33			! trap to kernel 

SRC_DST = 8			! source/ destination process 
MSGTAB = 8			! message table
MESSAGE = 12			! message pointer 
TABCOUNT = 12			! number of entries in message table

!*========================================================================*
!                           IPC assembly routines			  *
!*========================================================================*
! all message passing routines save ebp, but destroy eax and ecx.
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

__sendnb:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = dest-src
	mov	ebx, MESSAGE(ebp)	! ebx = message pointer
	mov	ecx, SENDNB		! _sendnb(dest, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__senda:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, TABCOUNT(ebp)	! eax = count
	mov	ebx, MSGTAB(ebp)	! ebx = table
	mov	ecx, SENDA		! _senda(table, count)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret
