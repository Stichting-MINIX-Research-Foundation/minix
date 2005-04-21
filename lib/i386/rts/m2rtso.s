! This is the Modula-2 run-time start-off routine.  It's job is to take the
! arguments as put on the stack by EXEC, and to parse them and set them up the
! way _m_a_i_n expects them.

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

.define m2rtso, hol0, __penviron, __penvp, __fpu_present
.sect .text
m2rtso:
	xor	ebp, ebp		! clear for backtrace of core files
	mov	eax, (esp)		! argc
	lea	edx, 4(esp)		! argv
	lea	ecx, 8(esp)(eax*4)	! envp

	! Test if environ is in the initialized data area and is set to our
	! magic number.  If so then it is not redefined by the user.
	mov	ebx, _environ
	cmp	ebx, __edata		! within initialized data?
	jae	0f
	testb	bl, 3			! aligned?
	jnz	0f
	cmp	(ebx), 0x53535353	! is it our environ?
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

	call	__m_a_i_n		! run Modula-2 program

	push	eax			! push exit status
	call	__exit

	hlt				! force a trap if exit fails

.sect .rom
	.data4	0			! Separate I&D: *NULL == 0
					! Also keeps the first string in the
					! program from appearing at location 0!
.sect .data
__penviron:
	.data4	__penvp			! Pointer to environ, or hidden pointer

.sect .bss
	.comm	__penvp, 4		! Hidden environment vector
	.comm	__fpu_present, 4	! FPU present flag

.extern endtext				! Force loading of end labels.
