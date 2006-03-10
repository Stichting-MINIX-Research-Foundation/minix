.sect .text; .sect .rom; .sect .data; .sect .bss
.define __ipc_request, __ipc_reply, __ipc_notify, __ipc_receive

! See src/kernel/ipc.h for C definitions.
IPC_REQUEST = 16		! each gets a distinct bit
IPC_REPLY = 32
IPC_NOTIFY = 64
IPC_RECEIVE = 128

SYSVEC = 33			! trap to kernel 

! Offsets of arguments relative to stack pointer.
SRC_DST = 8			! source/ destination process 
SEND_MSG = 12			! message pointer for sending 
EVENT_SET = 12			! notification event set 
RECV_MSG = 16			! message pointer for receiving 


!*========================================================================*
!                           IPC assembly routines			  *
!*========================================================================*
! all message passing routines save ebp, but destroy eax, ecx, and edx.
.define __ipc_request, __ipc_reply, __ipc_notify, __ipc_receive
.sect .text

__ipc_request:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = destination
	mov	ebx, SEND_MSG(ebp)	! ebx = message pointer
	mov	ecx, IPC_REQUEST	! _ipc_request(dst, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__ipc_reply:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = destination
	mov	ebx, SEND_MSG(ebp)	! ebx = message pointer
	mov	ecx, IPC_REPLY		! _ipc_reply(dst, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__ipc_receive:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! eax = source
	mov	edx, EVENT_SET(ebp)	! ebx = event set
	mov	ebx, RCV_MSG(ebp)	! ebx = message pointer
	mov	ecx, IPC_RECEIVE	! _ipc_receive(src, events, ptr)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret

__ipc_notify:
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	eax, SRC_DST(ebp)	! ebx = destination 
	mov	edx, EVENT_SET(ebp)	! edx = event set 
	mov	ecx, IPC_NOTIFY		! _ipc_notify(dst, events)
	int	SYSVEC			! trap to the kernel
	pop	ebx
	pop	ebp
	ret


