#
! This is the C run-time start-off routine.  It's job is to take the
! arguments as put on the stack by EXEC, and to parse them and set them up the
! way _main expects them.
! It also initializes _environ when this variable isn't defined by the
! programmer.  The detection of whether _environ belong to us is rather
! simplistic.  We simply check for some magic value, but there is no other
! way.

#include <sys/vm_i386.h>

.sect .text; .sect .rom; .sect .data; .sect .bss

.define begtext, begdata, begbss
.sect .text
begtext:
.sect .rom
begrom:
.sect .data
begdata:
.sect .bss
begbss:

.define crtso, __penviron, __penvp, __fpu_present
.define __minix_datastart, __minix_mainjump, __minix_unmapzero
.extern _main, _exit
.sect .text
crtso:
	xor     ebp, ebp		! clear for backtrace of core files
	mov     eax, (esp)		! argc
	lea     edx, 4(esp)		! argv
	lea     ecx, 8(esp)(eax*4)	! envp

	! Test if environ is in the initialized data area and is set to our
	! magic number.  If so then it is not redefined by the user.
	mov	ebx, _environ
	cmp	ebx, __edata		! within initialized data?
	jae	0f
	testb	bl, 3			! aligned?
	jnz	0f
	cmp	(ebx), 0x53535353	! is it our _environ?
	jne	0f
	mov	(__penviron), ebx	! _penviron = &environ;
0:	mov	ebx, (__penviron)
	mov	(ebx), ecx		! *_penviron = envp;

	push	ecx			! push envp
	push	edx			! push argv
	push	eax			! push argc

	! Test the EM bit of the MSW to determine if an FPU is present and
	! set __fpu_present if one is found.
	smsw	ax
	testb	al, 0x4			! EM bit in MSW
	setz	(__fpu_present)		! True if not set
	jmp __minix_mainjump

.align I386_PAGE_SIZE
__minix_mainjump:
	! unmap zero pages
	call	__minix_unmapzero

	call	_main			! main(argc, argv, envp)

	push	eax			! push exit status
	call	_exit

	hlt				! force a trap if exit fails

__minix_unmapzero:

	! unmap 0-page code
	push	I386_PAGE_SIZE
	push	crtso
	call	_munmap_text		! munmap_text(crtso, I386_PAGE_SIZE)
	add	esp, 8

	! unmap 0-page data
	push	I386_PAGE_SIZE
	push	romstart
	call	_munmap			! munmap(romstart, I386_PAGE_SIZE)
	add	esp, 8

	ret

.sect .rom
romstart:
	.space	I386_PAGE_SIZE
__minix_datastart:
	.space	4
.sect .data
__penviron:
	.data4	__penvp			! Pointer to environ, or hidden pointer

.sect .bss
	.comm	__penvp, 4		! Hidden environment vector
	.comm	__fpu_present, 4	! FPU present flag

.extern endtext				! Force loading of end labels.
