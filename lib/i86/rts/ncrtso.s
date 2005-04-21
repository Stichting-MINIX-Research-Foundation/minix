! This is the C run-time start-off routine.  It's job is to take the
! arguments as put on the stack by EXEC, and to parse them and set them up the
! way _main expects them.
! It also initializes _environ when this variable isn't defined by the
! programmer.  The detection of whether _environ belong to us is rather
! simplistic.  We simply check for some magic value, but there is no other
! way.

.extern _main, _exit, crtso, __penviron, __penvp
.extern begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
crtso:
	xor	bp, bp			! clear for backtrace of core files
	mov	bx, sp
	mov	ax, (bx)		! argc
	lea	dx, 2(bx)		! argv
	lea	cx, 4(bx)
	add	cx, ax
	add	cx, ax			! envp

	! Test if environ is in the initialized data area and is set to our
	! magic number.  If so then it is not redefined by the user.
	mov	bx, #_environ
	cmp	bx, #__edata		! within initialized data?
	jae	0f
	testb	bl, #1			! aligned?
	jnz	0f
	cmp	(bx), #0x5353		! is it our environ?
	jne	0f
	mov	__penviron, bx		! _penviron = &environ;
0:	mov	bx, __penviron
	mov	(bx), cx		! *_penviron = envp;

	push	cx			! push envp
	push	dx			! push argv
	push	ax			! push argc

	call	_main			! main(argc, argv, envp)

	push	ax			! push exit status
	call	_exit

	hlt				! force a trap if exit fails

.data
begdata:
	.data2	0			! for sep I&D: *NULL == 0
__penviron:
	.data2	__penvp			! Pointer to environ, or hidden pointer

.bss
begbss:
	.comm	__penvp, 2		! Hidden environment vector
