!	Doshead.s - DOS & BIOS support for boot.c	Author: Kees J. Bot
!
!
! This file contains the startup and low level support for the secondary
! boot program.  It contains functions for disk, tty and keyboard I/O,
! copying memory to arbitrary locations, etc.
!
! This runs under MS-DOS as a .COM file.  A .COM file is what Minix calls
! a common I&D executable, except that the first 256 bytes contains DOS
! thingies.
!
.sect .text; .sect .rom; .sect .data; .sect .bss

	K_I386	    =	0x0001	! Call Minix in 386 mode
	STACK	    =	 16384	! Number of bytes for the stack

	DS_SELECTOR =	   3*8	! Kernel data selector
	ES_SELECTOR =	   4*8	! Flat 4 Gb
	SS_SELECTOR =	   5*8	! Monitor stack
	CS_SELECTOR =	   6*8	! Kernel code
	MCS_SELECTOR=	   7*8	! Monitor code

	ESC	    =	  0x1B	! Escape character

! Imported variables and functions:
.extern _caddr, _daddr, _runsize, _edata, _end	! Runtime environment
.extern _k_flags				! Special kernel flags
.extern _mem					! Free memory list
.extern _vdisk					! Name of the virtual disk

.sect .text

.use16				! Tell 386 assembler we're in 16-bit mode

.define _PSP
_PSP:
	.space	256		! Program Segment Prefix

dosboot:
	cld			! C compiler wants UP
	xor	ax, ax		! Zero
	mov	di, _edata	! Start of bss is at end of data
	mov	cx, _end	! End of bss (begin of heap)
	sub	cx, di		! Number of bss bytes
	shr	cx, 1		! Number of words
 rep	stos			! Clear bss
	cmp	sp, _end+STACK
	jb	0f
	mov	sp, _end+STACK	! "chmem" to 16 kb
0:

! Are we called with the /U option?
	movb	cl, (_PSP+0x80)	! Argument byte count
	xorb	ch, ch
	mov	bx, _PSP+0x81	! Argument string
0:	jcxz	notuflag
	cmpb	(bx), 0x20	! Whitespace?
	ja	1f
	inc	bx
	dec	cx
	jmp	0b
1:	cmp	cx, 2		! '/U' is two bytes
	jne	notuflag
	cmpb	(bx), 0x2F	! '/'?
	jne	notuflag
	movb	al, 1(bx)
	andb	al, ~0x20	! Ignore case
	cmpb	al, 0x55	! 'U'?
	jne	notuflag
	jmp	keepumb		! Go grab an UMB
notuflag:

! Remember the current video mode for restoration on exit.
	movb	ah, 0x0F	! Get current video mode
	int	0x10
	andb	al, 0x7F	! Mask off bit 7 (no blanking)
	movb	(old_vid_mode), al
	movb	(cur_vid_mode), al

! We require at least MS-DOS 3.0.
	mov	ax, 0x3000	! Get DOS version
	int	0x21
	cmpb	al, 3		! DOS 3.0+ ?
	jae	dosok
	push	tellbaddos
	call	_printf
	jmp	quit
.sect	.rom
tellbaddos:	.ascii	"MS-DOS 3.0 or better required\n\0"
.sect	.text
dosok:

! Find out how much "low" memory there is available, where it starts and
! where it ends.
	mov	di, _mem		! di = memory list
	mov	ax, _PSP+0x80		! From PSP:80 to next PSP is ours
	mov	dx, ds
	call	seg2abs
	mov	(di), ax
	mov	2(di), dx		! mem[0].base = ds * 16 + 0x80
	xor	ax, ax
	mov	dx, (_PSP+2)		! First in-use segment far above us
	call	seg2abs
	sub	ax, (di)
	sbb	dx, 2(di)		! Minus base gives size
	mov	4(di), ax
	mov	6(di), dx		! mem[1].size = free low memory size

! Give C code access to the code segment, data segment and the size of this
! process.
	xor	ax, ax
	mov	dx, cs
	call	seg2abs
	mov	(_caddr+0), ax
	mov	(_caddr+2), dx
	xor	ax, ax
	mov	dx, ds
	call	seg2abs
	mov	(_daddr+0), ax
	mov	(_daddr+2), dx
	mov	ax, sp
	mov	dx, ss			! End of stack = end of program
	call	seg2abs
	sub	ax, (_caddr+0)
	sbb	dx, (_caddr+2)		! Minus start of our code
	mov	(_runsize+0), ax
	mov	(_runsize+2), dx	! Is our size

! Patch the regular _getprocessor library routine to jump to 'getprocessor',
! that checks if we happen to be in a V8086 straightjacket by returning '86'.
  cseg	movb	(_getprocessor+0), 0xE9
	mov	ax, getprocessor
	sub	ax, _getprocessor+3
  cseg	mov	(_getprocessor+1), ax

! Grab the largest chunk of extended memory available.
	call	_getprocessor
	cmp	ax, 286		! Only 286s and above have extended memory
	jb	no_ext
	mov	ax, 0x4300	! XMS driver check
	int	0x2F
	cmpb	al, 0x80	! XMS driver exists?
	je	xmsthere
get_ext:			! No driver, so can use all ext memory directly
	call	_getprocessor
	cmp	ax, 486		! Assume 486s were the first to have >64M
	jb	small_ext	! (It helps to be paranoid when using the BIOS)
big_ext:
	mov	ax, 0xE801	! Code for get memory size for >64M
	int	0x15		! ax = mem at 1M per 1K, bx = mem at 16M per 64K
	jnc	got_ext
small_ext:
	movb	ah, 0x88	! Code for get extended memory size
	clc			! Carry will stay clear if call exists
	int	0x15		! Returns size (in K) in ax for AT's
	jc	no_ext
	test	ax, ax		! An AT with no extended memory?
	jz	no_ext
	xor	bx, bx		! bx = mem above 16M per 64K = 0
got_ext:
	mov	cx, ax		! cx = copy of ext mem at 1M
	mov	10(di), 0x0010	! mem[1].base = 0x00100000 (1M)
	mul	(c1024)
	mov	12(di), ax	! mem[1].size = "ext mem at 1M" * 1024
	mov	14(di), dx
	test	bx, bx
	jz	no_ext		! No more ext mem above 16M?
	cmp	cx, 15*1024	! Chunks adjacent? (precisely 15M at 1M?)
	je	adj_ext
	mov	18(di), 0x0100	! mem[2].base = 0x01000000 (16M)
	mov	22(di), bx	! mem[2].size = "ext mem at 16M" * 64K
	jmp	no_ext
adj_ext:
	add	14(di), bx	! Add ext mem above 16M to mem below 16M
no_ext:
	jmp	gotxms

xmsthere:
	mov	ax, 0x4310		! Get XMS driver address
	int	0x2F
	mov	(xms_driver+0), bx
	mov	(xms_driver+2), es
	push	ds
	pop	es
	movb	ah, 0x08		! Query free extended memory
	xorb	bl, bl
	callf	(xms_driver)
	testb	bl, bl
	jnz	xmserr
	push	ax			! ax = size of largest block in kb
	mul	(c1024)
	mov	12(di), ax
	mov	14(di), dx		! mem[1].size = ax * 1024
	pop	dx			! dx = size of largest block in kb
	movb	ah, 0x09 		! Allocate XMS block of size dx
	callf	(xms_driver)
	test	ax, ax
	jz	xmserr
	mov	(xms_handle), dx	! Save handle
	movb	ah, 0x0C		! Lock XMS block (handle in dx)
	callf	(xms_driver)
	test	ax, ax
	jz	xmserr
	mov	8(di), bx
	mov	10(di), dx		! mem[1].base = Address of locked block
gotxms:

! If we're running in a DOS box then they're might be an Upper Memory Block
! we can use.  Every little bit helps when in real mode.
	mov	ax, 20(di)
	or	ax, 22(di)		! Can we use mem[2]?
	jnz	gotumb
	mov	dx, 0xFFFF		! dx = Maximum size, i.e. gimme all
	call	getumb			! Get UMB, dx = segment, cx = length
	test	cx, cx			! Did we get a block?
	jz	gotumb
	xor	ax, ax			! dx:ax = memory block
	call	seg2abs
	mov	16(di), ax
	mov	18(di), dx		! mem[2].base = memory block base
	mov	dx, cx
	xor	ax, ax			! dx:ax = length of memory block
	call	seg2abs
	mov	20(di), ax
	mov	22(di), dx		! mem[2].size = memory block length
gotumb:

! Set up an INT 24 "critical error" handler that returns "fail".  This way
! Minix won't suffer from "(A)bort, (R)etry, (I)nfluence with a large hammer?".
	mov	(0x007C), 0x03B0	! movb al, 0x03 (fail code)
	movb	(0x007E), 0xCF		! iret
	movb	ah, 0x25		! Set interrupt vector
	mov	dx, 0x007C		! ds:dx = ds:0x007C = interrupt handler
	int	0x21

! Time to switch to a higher level language (not much higher)
	call	_boot

! void ..exit(int status)
!	Exit the monitor by returning to DOS.
.define	_exit, __exit, ___exit		! Make various compilers happy
_exit:
__exit:
___exit:
	mov	dx, (xms_handle)
	cmp	dx, -1			! Is there an ext mem block in use?
	je	nohandle
	movb	ah, 0x0D		! Unlock extended memory block
	callf	(xms_driver)
	mov	dx, (xms_handle)
	movb	ah, 0x0A		! Free extended memory block
	callf	(xms_driver)
nohandle:
	call	restore_video
	pop	ax
	pop	ax			! Return code in al
	movb	ah, 0x4C		! Terminate with return code
	int	0x21

quit:					! exit(1)
	movb	al, 1
	push	ax
	call	_exit

xmserr:
	xorb	bh, bh
	push	bx
	push	tellxmserr
	call	_printf
	jmp	quit
.sect	.rom
tellxmserr:	.ascii	"Extended memory problem, error 0x%02x\n\0"
.sect	.text

! int getprocessor(void)
!	Prefix for the regular _getprocessor call that first checks if we're
!	running in a virtual 8086 box.
getprocessor:
	push	sp		! Is pushed sp equal to sp?
	pop	ax
	cmp	ax, sp
	jne	gettrueproc	! If not then it's a plain 8086 or 80186
	.data1	0x0F,0x01,0xE0	! Use old 286 SMSW instruction to get the MSW
	testb	al, 0x01	! Protected mode enabled?
	jz	gettrueproc	! If not then a 286 or better in real mode
	mov	ax, 86		! Forget fancy tricks, say it's an 8086
	ret
gettrueproc:			! Get the true processor type
	push	bp		! _getprocessor prologue that is patched over.
	mov	bp, sp
	jmp	_getprocessor+3

! Try to get an Upper Memory Block under MS-DOS 5+.  Try to get one up to size
! dx, return segment of UMB found in dx and size in paragraphs in cx.
getumb:
	xor	cx, cx			! Initially nothing found
	mov	ax, 0x3000		! Get DOS version
	int	0x21
	cmpb	al, 5			! MS-DOS 5.0 or better?
	jb	retumb
	mov	ax, 0x544D		! Get UMB kept by BOOT /U
	int	0x15			! Returns dx = segment, cx = size
	jc	0f
	cmp	ax, 0x4D54		! Carry clear and ax byte swapped?
	je	retumb
0:	mov	ax, 0x5802		! Get UMB link state
	int	0x21
	xorb	ah, ah
	push	ax			! Save UMB link state 
	mov	ax, 0x5803		! Set UMB link state
	mov	bx, 0x0001		! Add UMBs to DOS memory chain
	int	0x21
	mov	ax, 0x5800		! Get memory allocation strategy
	int	0x21
	push	ax			! Save allocation strategy
	mov	ax, 0x5801		! Set memory allocation strategy
	mov	bx, 0x0080		! First fit, try high then low memory
	int	0x21
	movb	ah, 0x48		! Allocate memory
	mov	bx, dx			! Number of paragraphs wanted
	int	0x21			! Fails with bx = size of largest
	jnc	0f			! Succeeds with ax = allocated block
	test	bx, bx			! Is there any?
	jz	no_umb
	movb	ah, 0x48		! Allocate memory
	int	0x21
	jc	no_umb			! Did we get some?
0:	mov	dx, ax			! dx = segment
	mov	cx, bx			! cx = size
no_umb:	mov	ax, 0x5801		! Set memory allocation strategy
	pop	bx			! bx = saved former strategy
	int	0x21
	mov	ax, 0x5803		! Set UMB link state
	pop	bx			! bx = saved former link state
	int	0x21
retumb:	ret

! 'BOOT /U' instructs this program to grab the biggest available UMB and to
! sit on it until the next invocation of BOOT wants it back.  These shenanigans
! are necessary because Windows 95 keeps all UMBs to itself unless you get hold
! of them first.
	umb =	0x80			! UMB base and size
	old15 =	0x84			! Old 15 interrupt vector
	new15 = 0x88			! New 15 interrupt handler
keepumb:
	mov	ax, 0x544D		! "Keep UMB" handler already present?
	int	0x15
	jc	0f
	cmp	ax, 0x4D54
	je	exitumb			! Already present, so quit
0:
	mov	si, new15start
	mov	di, new15
	mov	cx, new15end
	sub	cx, si
  rep	movsb				! Copy handler into place
	add	di, 15
	movb	cl, 4
	shr	di, cl			! di = first segment above handler
	mov	cx, cs
	cmp	cx, 0xA000		! Are we loaded high perchance?
	jb	nothigh
werehigh:
	add	cx, di
	mov	(umb+0), cx		! Use my own memory as the UMB to keep
	mov	ax, (_PSP+2)		! Up to the next in-use segment
	sub	ax, dx			! ax = size of my free memory
	cmp	ax, 0x1000		! At least 64K?
	jb	exitumb			! Don't bother if less
	mov	(umb+2), 0x1000		! Size of UMB
	add	di, 0x1000		! Keep my code plus 64K when TSR
	jmp	hook15
nothigh:
	mov	dx, 0x1000
	call	getumb			! Grab an UMB of at most 64K
	cmp	cx, 0x1000		! Did we get 64K?
	jb	exitumb			! Otherwise don't bother
	mov	(umb+0), dx
	mov	(umb+2), cx
hook15:
	mov	ax, 0x3515		! Get interrupt vector
	int	0x21
	mov	(old15+0), bx
	mov	(old15+2), es		! Old 15 interrupt
	mov	ax, 0x2515		! Set interrupt vector
	mov	dx, new15		! ds:dx = new 15 handler
	int	0x21
	mov	ax, 0x3100		! Terminate and stay resident
	mov	dx, di			! dx = di = paragraphs we keep
	int	0x21
exitumb:
	mov	ax, 0x4C00		! exit(0)
	int	0x21

new15start:				! New interrupt 15 handler
	pushf
	cmp	ax, 0x544D		! Is it my call?
	je	my15
	popf
  cseg	jmpf	(old15)			! No, continue with old 15
my15:	popf
	push	bp
	mov	bp, sp
	andb	6(bp), ~0x01		! clear carry, call will succeed
	xchgb	al, ah			! ax = 4D54, also means call works
  cseg	mov	dx, (umb+0)		! dx = base of UMB
  cseg	mov	cx, (umb+2)		! cx = size of UMB
	pop	bp
	iret				! return to caller
new15end:

! u32_t mon2abs(void *ptr)
!	Address in monitor data to absolute address.
.define _mon2abs
_mon2abs:
	mov	bx, sp
	mov	ax, 2(bx)	! ptr
	mov	dx, ds		! Monitor data segment
	!jmp	seg2abs

seg2abs:			! Translate dx:ax to the 32 bit address dx-ax
	push	cx
	movb	ch, dh
	movb	cl, 4
	shl	dx, cl
	shrb	ch, cl		! ch-dx = dx << 4
	add	ax, dx
	adcb	ch, 0		! ch-ax = ch-dx + ax
	movb	dl, ch
	xorb	dh, dh		! dx-ax = ch-ax
	pop	cx
	ret

abs2seg:			! Translate the 32 bit address dx-ax to dx:ax
	push	cx
	movb	ch, dl
	mov	dx, ax		! ch-dx = dx-ax
	and	ax, 0x000F	! Offset in ax
	movb	cl, 4
	shr	dx, cl
	shlb	ch, cl
	orb	dh, ch		! dx = ch-dx >> 4
	pop	cx
	ret

! void raw_copy(u32_t dstaddr, u32_t srcaddr, u32_t count)
!	Copy count bytes from srcaddr to dstaddr.  Don't do overlaps.
!	Also handles copying words to or from extended memory.
.define _raw_copy
_raw_copy:
	push	bp
	mov	bp, sp
	push	si
	push	di		! Save C variable registers
copy:
	cmp	14(bp), 0
	jnz	bigcopy
	mov	cx, 12(bp)
	jcxz	copydone	! Count is zero, end copy
	cmp	cx, 0xFFF0
	jb	smallcopy
bigcopy:mov	cx, 0xFFF0	! Don't copy more than about 64K at once
smallcopy:
	push	cx		! Save copying count
	mov	ax, 4(bp)
	mov	dx, 6(bp)
	cmp	dx, 0x0010	! Copy to extended memory?
	jae	ext_copy
	cmp	10(bp), 0x0010	! Copy from extended memory?
	jae	ext_copy
	call	abs2seg
	mov	di, ax
	mov	es, dx		! es:di = dstaddr
	mov	ax, 8(bp)
	mov	dx, 10(bp)
	call	abs2seg
	mov	si, ax
	mov	ds, dx		! ds:si = srcaddr
	shr	cx, 1		! Words to move
 rep	movs			! Do the word copy
	adc	cx, cx		! One more byte?
 rep	movsb			! Do the byte copy
	mov	ax, ss		! Restore ds and es from the remaining ss
	mov	ds, ax
	mov	es, ax
 	jmp	copyadjust
ext_copy:
	mov	(x_dst_desc+2), ax
	movb	(x_dst_desc+4), dl ! Set base of destination segment
	mov	ax, 8(bp)
	mov	dx, 10(bp)
	mov	(x_src_desc+2), ax
	movb	(x_src_desc+4), dl ! Set base of source segment
	mov	si, x_gdt	! es:si = global descriptor table
	shr	cx, 1		! Words to move
	movb	ah, 0x87	! Code for extended memory move
	int	0x15
copyadjust:
	pop	cx		! Restore count
	add	4(bp), cx
	adc	6(bp), 0	! srcaddr += copycount
	add	8(bp), cx
	adc	10(bp), 0	! dstaddr += copycount
	sub	12(bp), cx
	sbb	14(bp), 0	! count -= copycount
	jmp	copy		! and repeat
copydone:
	pop	di
	pop	si		! Restore C variable registers
	pop	bp
	ret

! u16_t get_word(u32_t addr);
! void put_word(u32_t addr, u16_t word);
!	Read or write a 16 bits word at an arbitrary location.
.define	_get_word, _put_word
_get_word:
	mov	bx, sp
	call	gp_getaddr
	mov	ax, (bx)	! Word to get from addr
	jmp	gp_ret
_put_word:
	mov	bx, sp
	push	6(bx)		! Word to store at addr
	call	gp_getaddr
	pop	(bx)		! Store the word
	jmp	gp_ret
gp_getaddr:
	mov	ax, 2(bx)
	mov	dx, 4(bx)
	call	abs2seg
	mov	bx, ax
	mov	ds, dx		! ds:bx = addr
	ret
gp_ret:
	push	es
	pop	ds		! Restore ds
	ret

! void relocate(void);
!	After the program has copied itself to a safer place, it needs to change
!	the segment registers.  Caddr has already been set to the new location.
.define _relocate
_relocate:
	pop	bx		! Return address
	mov	ax, (_caddr+0)
	mov	dx, (_caddr+2)
	call	abs2seg
	mov	cx, dx		! cx = new code segment
	mov	ax, cs		! Old code segment
	sub	ax, cx		! ax = -(new - old) = -Moving offset
	mov	dx, ds
	sub	dx, ax
	mov	ds, dx		! ds += (new - old)
	mov	es, dx
	mov	ss, dx
	xor	ax, ax
	call	seg2abs
	mov	(_daddr+0), ax
	mov	(_daddr+2), dx	! New data address
	push	cx		! New text segment
	push	bx		! Return offset of this function
	retf			! Relocate

! void *brk(void *addr)
! void *sbrk(size_t incr)
!	Cannot fail implementations of brk(2) and sbrk(3), so we can use
!	malloc(3).  They reboot on stack collision instead of returning -1.
.sect .data
	.align	2
break:	.data2	_end		! A fake heap pointer
.sect .text
.define _brk, __brk, _sbrk, __sbrk
_brk:
__brk:				! __brk is for the standard C compiler
	xor	ax, ax
	jmp	sbrk		! break= 0; return sbrk(addr);
_sbrk:
__sbrk:
	mov	ax, (break)	! ax= current break
sbrk:	push	ax		! save it as future return value
	mov	bx, sp		! Stack is now: (retval, retaddr, incr, ...)
	add	ax, 4(bx)	! ax= break + increment
	mov	(break), ax	! Set new break
	lea	dx, -1024(bx)	! sp minus a bit of breathing space
	cmp	dx, ax		! Compare with the new break
	jb	heaperr		! Suffocating noises
	pop	ax		! Return old break (0 for brk)
	ret
heaperr:push	nomem
	call	_printf
	call	quit
.sect .rom
nomem:	.ascii	"\nOut of memory\n\0"
.sect .text

! int dev_open(void);
!	Open file 'vdisk' to use as the Minix virtual disk.  Store handle in
!	vfd.  Returns 0 for success, otherwise the DOS error code.
.define _dev_open
_dev_open:
	call	_dev_close	! If already open then first close
	mov	dx, (_vdisk)	! ds:dx = Address of file name
	mov	ax, 0x3D22	! Open file read-write & deny write
	int	0x21
	jnc	opok		! Open succeeded?
	cmp	ax, 5		! Open failed, "access denied"?
	jne	opbad
	mov	ax, 0x3D40	! Open file read-only
	int	0x21
	jc	opbad
opok:	mov	(vfd), ax	! File handle to open file
	xor	ax, ax		! Zero for success
opbad:	ret

! int dev_close(void);
!	Close the dos virtual disk.
.define _dev_close
_dev_close:
	mov	bx, -1
	cmp	(vfd), bx	! Already closed?
	je	1f
	movb	ah, 0x3E	! Close file
	xchg	bx, (vfd)	! bx = vfd; vfd = -1;
	int	0x21
	jc	0f
1:	xor	ax, ax
0:	ret

! int dev_boundary(u32_t sector);
!	Returns false; files have no visible boundaries.
.define	_dev_boundary
_dev_boundary:
	xor	ax, ax
	ret

! int readsectors(u32_t bufaddr, u32_t sector, u8_t count)
! int writesectors(u32_t bufaddr, u32_t sector, u8_t count)
!	Read/write several sectors from/to the Minix virtual disk.  Count
!	must fit in a byte.  The external variable vfd is the file handle.
!	Returns 0 for success, otherwise the DOS error code.
!
.define _readsectors, _writesectors
_writesectors:
	push	bp
	mov	bp, sp
	movb	13(bp), 0x40	! Code for a file write
	jmp	rwsec
_readsectors:
	push	bp
	mov	bp, sp
	movb	13(bp), 0x3F	! Code for a file read
rwsec:
	cmp	(vfd), -1	! Currently closed?
	jne	0f
	call	_dev_open	! Open file if needed
	test	ax, ax
	jnz	rwerr
0:	mov	dx, 8(bp)
	mov	bx, 10(bp)	! bx-dx = Sector number
	mov	cx, 9
mul512:	shl	dx, 1
	rcl	bx, 1		! bx-dx *= 512
	loop	mul512
	mov	cx, bx		! cx-dx = Byte position in file
	mov	bx, (vfd)	! bx = File handle
	mov	ax, 0x4200	! Lseek absolute
	int	0x21
	jb	rwerr
	mov	bx, (vfd)	! bx = File handle
	mov	ax, 4(bp)
	mov	dx, 6(bp)	! dx-ax = Address to transfer data to/from
	call	abs2seg
	mov	ds, dx
	mov	dx, ax		! ds:dx = Address to transfer data to/from
	xorb	cl, cl
	movb	ch, 12(bp)	! ch = Number of sectors to transfer
	shl	cx, 1		! cx = Number of bytes to transfer
	push	cx		! Save count
	movb	ah, 13(bp)	! Read or write
	int	0x21
	pop	cx		! Restore count
	push	es
	pop	ds		! Restore ds
	jb	rwerr
	cmp	ax, cx		! All bytes transferred?
	je	rwall
	mov	ax, 0x05	! The DOS code for "I/O error", but different
	jmp	rwerr
rwall:	call	wheel		! Display tricks
	xor	ax, ax
rwerr:	pop	bp
	ret

! int getch(void);
!	Read a character from the keyboard, and check for an expired timer.
!	A carriage return is changed into a linefeed for UNIX compatibility.
.define _getch
_getch:
	xor	ax, ax
	xchg	ax, (unchar)	! Ungotten character?
	test	ax, ax
	jnz	gotch
getch:	hlt			! Play dead until interrupted (see pause())
	movb	ah, 0x01	! Keyboard status
	int	0x16
	jnz	press		! Keypress?
	call	_expired	! Timer expired?
	test	ax, ax
	jz	getch
	mov	ax, ESC		! Return ESC
	ret
press:
	xorb	ah, ah		! Read character from keyboard
	int	0x16
	cmpb	al, 0x0D	! Carriage return?
	jnz	nocr
	movb	al, 0x0A	! Change to linefeed
nocr:	cmpb	al, ESC		! Escape typed?
	jne	noesc
	inc	(escape)	! Set flag
noesc:	xorb	ah, ah		! ax = al
gotch:	ret

! int ungetch(void);
!	Return a character to undo a getch().
.define _ungetch
_ungetch:
	mov	bx, sp
	mov	ax, 2(bx)
	mov	(unchar), ax
	ret

! int escape(void);
!	True if ESC has been typed.
.define _escape
_escape:
	movb	ah, 0x01	! Keyboard status
	int	0x16
	jz	escflg		! Keypress?
	cmpb	al, ESC		! Escape typed?
	jne	escflg
	xorb	ah, ah		! Discard the escape
	int	0x16
	inc	(escape)	! Set flag
escflg:	xor	ax, ax
	xchg	ax, (escape)	! Escape typed flag
	ret

! int putch(int c);
!	Write a character in teletype mode.  The putk synonym is
!	for the kernel printf function that uses it.
!	Newlines are automatically preceded by a carriage return.
!
.define _putch, _putk
_putch:
_putk:	mov	bx, sp
	movb	al, 2(bx)	! al = character to be printed
	testb	al, al		! Kernel printf adds a null char to flush queue
	jz	nulch
	cmpb	al, 0x0A	! al = newline?
	jnz	putc
	movb	al, 0x20	! Erase wheel and do a carriage return
	call	plotc		! plotc(' ');
nodirt:	movb	al, 0x0D
	call	putc		! putc('\r')
	movb	al, 0x0A	! Restore the '\n' and print it
putc:	movb	ah, 0x0E	! Print character in teletype mode
	mov	bx, 0x0001	! Page 0, foreground color
	int	0x10		! Call BIOS VIDEO_IO
nulch:	ret

! |/-\|/-\|/-\|/-\|/-\	(playtime)
wheel:	mov	bx, (gp)
	movb	al, (bx)
	inc	bx		! al = *gp++;
	cmp	bx, glyphs+4
	jne	0f
	mov	bx, glyphs
0:	mov	(gp), bx	! gp= gp == glyphs + 4 ? glyphs : gp;
	!jmp	plotc
plotc:	movb	ah, 0x0A	! 0x0A = write character at cursor
	mov	bx, 0x0001	! Page 0, foreground color
	mov	cx, 0x0001	! Just one character
	int	0x10
	ret
.sect .data
	.align	2
gp:	.data2	glyphs
glyphs:	.ascii	"|/-\\"
.sect .text

! void pause(void);
!	Wait for an interrupt using the HLT instruction.  This either saves
!	power, or tells an x86 emulator that nothing is happening right now.
.define _pause
_pause:
	hlt
	ret

! void set_mode(unsigned mode);
! void clear_screen(void);
!	Set video mode / clear the screen.
.define _set_mode, _clear_screen
_set_mode:
	mov	bx, sp
	mov	ax, 2(bx)	! Video mode
	cmp	ax, (cur_vid_mode)
	je	modeok		! Mode already as requested?
	mov	(cur_vid_mode), ax
_clear_screen:
	mov	ax, (cur_vid_mode)
	andb	ah, 0x7F	! Test bits 8-14, clear bit 15 (8x8 flag)
	jnz	xvesa		! VESA extended mode?
	int	0x10		! Reset video (ah = 0)
	jmp	mdset
xvesa:	mov	bx, ax		! bx = extended mode
	mov	ax, 0x4F02	! Reset video
	int	0x10
mdset:	testb	(cur_vid_mode+1), 0x80
	jz	setcur		! 8x8 font requested?
	mov	ax, 0x1112	! Load ROM 8 by 8 double-dot patterns
	xorb	bl, bl		! Load block 0
	int	0x10
setcur:	xor	dx, dx		! dl = column = 0, dh = row = 0
	xorb	bh, bh		! Page 0
	movb	ah, 0x02	! Set cursor position
	int	0x10
modeok:	ret

restore_video:			! To restore the video mode on exit
	movb	al, 0x20
	call	plotc		! Erase wheel
	push	(old_vid_mode)
	call	_set_mode
	pop	ax
	ret

! u32_t get_tick(void);
!	Return the current value of the clock tick counter.  This counter
!	increments 18.2 times per second.  Poll it to do delays.  Does not
!	work on the original PC, but works on the PC/XT.
.define _get_tick
_get_tick:
	xorb	ah, ah		! Code for get tick count
	int	0x1A
	mov	ax, dx
	mov	dx, cx		! dx:ax = cx:dx = tick count
	ret


! Functions used to obtain info about the hardware.  Boot uses this information
! itself, but will also pass them on to a pure 386 kernel, because one can't
! make BIOS calls from protected mode.  The video type could probably be
! determined by the kernel too by looking at the hardware, but there is a small
! chance on errors that the monitor allows you to correct by setting variables.

.define _get_bus		! returns type of system bus
.define _get_video		! returns type of display

! u16_t get_bus(void)
!	Return type of system bus, in order: XT, AT, MCA.
_get_bus:
	call	gettrueproc
	xor	dx, dx		! Assume XT
	cmp	ax, 286		! An AT has at least a 286
	jb	got_bus
	inc	dx		! Assume AT
	movb	ah, 0xC0	! Code for get configuration
	int	0x15
	jc	got_bus		! Carry clear and ah = 00 if supported
	testb	ah, ah
	jne	got_bus
 eseg	movb	al, 5(bx)	! Load feature byte #1
	inc	dx		! Assume MCA
	testb	al, 0x02	! Test bit 1 - "bus is Micro Channel"
	jnz	got_bus
	dec	dx		! Assume AT
	testb	al, 0x40	! Test bit 6 - "2nd 8259 installed"
	jnz	got_bus
	dec	dx		! It is an XT
got_bus:
	push	ds
	pop	es		! Restore es
	mov	ax, dx		! Return bus code
	mov	(bus), ax	! Keep bus code, A20 handler likes to know
	ret

! u16_t get_video(void)
!	Return type of display, in order: MDA, CGA, mono EGA, color EGA,
!	mono VGA, color VGA.
_get_video:
	mov	ax, 0x1A00	! Function 1A returns display code
	int	0x10		! al = 1A if supported
	cmpb	al, 0x1A
	jnz	no_dc		! No display code function supported

	mov	ax, 2
	cmpb	bl, 5		! Is it a monochrome EGA?
	jz	got_video
	inc	ax
	cmpb	bl, 4		! Is it a color EGA?
	jz	got_video
	inc	ax
	cmpb	bl, 7		! Is it a monochrome VGA?
	jz	got_video
	inc	ax
	cmpb	bl, 8		! Is it a color VGA?
	jz	got_video

no_dc:	movb	ah, 0x12	! Get information about the EGA
	movb	bl, 0x10
	int	0x10
	cmpb	bl, 0x10	! Did it come back as 0x10? (No EGA)
	jz	no_ega

	mov	ax, 2
	cmpb	bh, 1		! Is it monochrome?
	jz	got_video
	inc	ax
	jmp	got_video

no_ega:	int	0x11		! Get bit pattern for equipment
	and	ax, 0x30	! Isolate color/mono field
	sub	ax, 0x30
	jz	got_video	! Is it an MDA?
	mov	ax, 1		! No it's CGA

got_video:
	ret


! Function to leave the boot monitor and run Minix.
.define _minix

! void minix(u32_t koff, u32_t kcs, u32_t kds,
!				char *bootparams, size_t paramsize, u32_t aout);
_minix:
	push	bp
	mov	bp, sp		! Pointer to arguments

	mov	dx, 0x03F2	! Floppy motor drive control bits
	movb	al, 0x0C	! Bits 4-7 for floppy 0-3 are off
	outb	dx		! Kill the motors
	push	ds
	xor	ax, ax		! Vector & BIOS data segments
	mov	ds, ax
	andb	(0x043F), 0xF0	! Clear diskette motor status bits of BIOS
	pop	ds
	cli			! No more interruptions

	test	(_k_flags), K_I386 ! Minix-386?
	jnz	minix386

! Call Minix in real mode.
minix86:
	push	22(bp)		! Address of a.out headers
	push	20(bp)

	push	18(bp)		! # bytes of boot parameters
	push	16(bp)		! Address of boot parameters

	mov	dx, cs		! Monitor far return address
	mov	ax, ret86
	cmp	(_mem+14), 0	! Any extended memory?  (mem[1].size > 0 ?)
	jnz	0f
	xor	dx, dx		! If no ext mem then monitor not preserved
	xor	ax, ax
0:	push	dx		! Push monitor far return address or zero
	push	ax

	mov	ax, 8(bp)
	mov	dx, 10(bp)
	call	abs2seg
	push	dx		! Kernel code segment
	push	4(bp)		! Kernel code offset
	mov	ax, 12(bp)
	mov	dx, 14(bp)
	call	abs2seg
	mov	ds, dx		! Kernel data segment
	mov	es, dx		! Set es to kernel data too
	retf			! Make a far call to the kernel

! Call 386 Minix in 386 mode.
minix386:
  cseg	mov	(cs_real-2), cs	! Patch CS and DS into the instructions that
  cseg	mov	(ds_real-2), ds	! reload them when switching back to real mode
	mov	eax, cr0
	orb	al, 0x01	! Set PE (protection enable) bit
   o32	mov	(msw), eax	! Save as protected mode machine status word

	mov	dx, ds		! Monitor ds
	mov	ax, p_gdt	! dx:ax = Global descriptor table
	call	seg2abs
	mov	(p_gdt_desc+2), ax
	movb	(p_gdt_desc+4), dl ! Set base of global descriptor table

	mov	ax, 12(bp)
	mov	dx, 14(bp)	! Kernel ds (absolute address)
	mov	(p_ds_desc+2), ax
	movb	(p_ds_desc+4), dl ! Set base of kernel data segment

	mov	dx, ss		! Monitor ss
	xor	ax, ax		! dx:ax = Monitor stack segment
	call	seg2abs		! Minix starts with the stack of the monitor
	mov	(p_ss_desc+2), ax
	movb	(p_ss_desc+4), dl

	mov	ax, 8(bp)
	mov	dx, 10(bp)	! Kernel cs (absolute address)
	mov	(p_cs_desc+2), ax
	movb	(p_cs_desc+4), dl

	mov	dx, cs		! Monitor cs
	xor	ax, ax		! dx:ax = Monitor code segment
	call	seg2abs
	mov	(p_mcs_desc+2), ax
	movb	(p_mcs_desc+4), dl

	push	MCS_SELECTOR
	push	int86		! Far address to INT86 support

   o32	push	20(bp)		! Address of a.out headers

	push	0
	push	18(bp)		! 32 bit size of parameters on stack
	push	0
	push	16(bp)		! 32 bit address of parameters (ss relative)

	push	MCS_SELECTOR
	push	ret386		! Monitor far return address

	push	0
	push	CS_SELECTOR
	push	6(bp)
	push	4(bp)		! 32 bit far address to kernel entry point

	call	real2prot	! Switch to protected mode
	mov	ax, DS_SELECTOR
	mov	ds, ax		! Kernel data
	mov	ax, ES_SELECTOR
	mov	es, ax		! Flat 4 Gb
   o32	retf			! Make a far call to the kernel

! Minix-86 returns here on a halt or reboot.
ret86:
	mov	8(bp), ax
	mov	10(bp), dx	! Return value
	jmp	return

! Minix-386 returns here on a halt or reboot.
ret386:
   o32	mov	8(bp), eax	! Return value
	call	prot2real	! Switch to real mode

return:
	mov	sp, bp		! Pop parameters
	sti			! Can take interrupts again

	call	_get_video	! MDA, CGA, EGA, ...
	movb	dh, 24		! dh = row 24
	cmp	ax, 2		! At least EGA?
	jb	is25		! Otherwise 25 rows
	push	ds
	xor	ax, ax		! Vector & BIOS data segments
	mov	ds, ax
	movb	dh, (0x0484)	! Number of rows on display minus one
	pop	ds
is25:
	xorb	dl, dl		! dl = column 0
	xorb	bh, bh		! Page 0
	movb	ah, 0x02	! Set cursor position
	int	0x10

	xorb	ah, ah		! Whack the disk system, Minix may have messed
	movb	dl, 0x80	! it up
	int	0x13

	call	gettrueproc
	cmp	ax, 286
	jb	noclock
	xorb	al, al
tryclk:	decb	al
	jz	noclock
	movb	ah, 0x02	! Get real-time clock time (from CMOS clock)
	int	0x1A
	jc	tryclk		! Carry set, not running or being updated
	movb	al, ch		! ch = hour in BCD
	call	bcd		! al = (al >> 4) * 10 + (al & 0x0F)
	mulb	(c60)		! 60 minutes in an hour
	mov	bx, ax		! bx = hour * 60
	movb	al, cl		! cl = minutes in BCD
	call	bcd
	add	bx, ax		! bx = hour * 60 + minutes
	movb	al, dh		! dh = seconds in BCD
	call	bcd
	xchg	ax, bx		! ax = hour * 60 + minutes, bx = seconds
	mul	(c60)		! dx-ax = (hour * 60 + minutes) * 60
	add	bx, ax
	adc	dx, 0		! dx-bx = seconds since midnight
	mov	ax, dx
	mul	(c19663)
	xchg	ax, bx
	mul	(c19663)
	add	dx, bx		! dx-ax = dx-bx * (0x1800B0 / (2*2*2*2*5))
	mov	cx, ax		! (0x1800B0 = ticks per day of BIOS clock)
	mov	ax, dx
	xor	dx, dx
	div	(c1080)
	xchg	ax, cx
	div	(c1080)		! cx-ax = dx-ax / (24*60*60 / (2*2*2*2*5))
	mov	dx, ax		! cx-dx = ticks since midnight
	movb	ah, 0x01	! Set system time
	int	0x1A
noclock:

	mov	ax, 8(bp)
	mov	dx, 10(bp)	! dx-ax = return value from the kernel
	pop	bp
	ret			! Return to monitor as if nothing much happened

! Transform BCD number in al to a regular value in ax.
bcd:	movb	ah, al
	shrb	ah, 4
	andb	al, 0x0F
	aad			! ax = (al >> 4) * 10 + (al & 0x0F)
	ret

! Support function for Minix-386 to make an 8086 interrupt call.
int86:
	mov	bp, sp
	call	prot2real

   o32	xor	eax, eax
	mov	es, ax		! Vector & BIOS data segments
o32 eseg mov	(0x046C), eax	! Clear BIOS clock tick counter

	sti			! Enable interrupts

	movb	al, 0xCD	! INT instruction
	movb	ah, 8(bp)	! Interrupt number?
	testb	ah, ah
	jnz	0f		! Nonzero if INT, otherwise far call
	push	cs
	push	intret+2	! Far return address
   o32	push	12(bp)		! Far driver address
	mov	ax, 0x90CB	! RETF; NOP
0: cseg	mov	(intret), ax	! Patch `INT n' or `RETF; NOP' into code

	mov	ds, 16(bp)	! Load parameters
	mov	es, 18(bp)
   o32	mov	eax, 20(bp)
   o32	mov	ebx, 24(bp)
   o32	mov	ecx, 28(bp)
   o32	mov	edx, 32(bp)
   o32	mov	esi, 36(bp)
   o32	mov	edi, 40(bp)
   o32	mov	ebp, 44(bp)

intret:	int	0xFF		! Do the interrupt or far call

   o32	push	ebp		! Save results
   o32	pushf
	mov	bp, sp
   o32	pop	8+8(bp)		! eflags
	mov	8+16(bp), ds
	mov	8+18(bp), es
   o32	mov	8+20(bp), eax
   o32	mov	8+24(bp), ebx
   o32	mov	8+28(bp), ecx
   o32	mov	8+32(bp), edx
   o32	mov	8+36(bp), esi
   o32	mov	8+40(bp), edi
   o32	pop	8+44(bp)	! ebp

	cli			! Disable interrupts

	xor	ax, ax
	mov	ds, ax		! Vector & BIOS data segments
   o32	mov	cx, (0x046C)	! Collect lost clock ticks in ecx

	mov	ax, ss
	mov	ds, ax		! Restore monitor ds
	call	real2prot
	mov	ax, DS_SELECTOR	! Kernel data
	mov	ds, ax
   o32	retf			! Return to the kernel

! Switch from real to protected mode.
real2prot:
	movb	ah, 0x02	! Code for A20 enable
	call	gate_A20

	lgdt	(p_gdt_desc)	! Global descriptor table
   o32	mov	eax, (pdbr)	! Load page directory base register
	mov	cr3, eax
	mov	eax, cr0
   o32	xchg	eax, (msw)	! Exchange real mode msw for protected mode msw
	mov	cr0, eax
	jmpf	MCS_SELECTOR:cs_prot ! Set code segment selector
cs_prot:
	mov	ax, SS_SELECTOR	! Set data selectors
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	ret

! Switch from protected to real mode.
prot2real:
	lidt	(p_idt_desc)	! Real mode interrupt vectors
	mov	eax, cr3
   o32	mov	(pdbr), eax	! Save page directory base register
	mov	eax, cr0
   o32	xchg	eax, (msw)	! Exchange protected mode msw for real mode msw
	mov	cr0, eax
	jmpf	0xDEAD:cs_real	! Reload cs register
cs_real:
	mov	ax, 0xBEEF
ds_real:
	mov	ds, ax		! Reload data segment registers
	mov	es, ax
	mov	ss, ax

	xorb	ah, ah		! Code for A20 disable
	!jmp	gate_A20

! Enable (ah = 0x02) or disable (ah = 0x00) the A20 address line.
gate_A20:
	cmp	(bus), 2	! PS/2 bus?
	je	gate_PS_A20
	call	kb_wait
	movb	al, 0xD1	! Tell keyboard that a command is coming
	outb	0x64
	call	kb_wait
	movb	al, 0xDD	! 0xDD = A20 disable code if ah = 0x00
	orb	al, ah		! 0xDF = A20 enable code if ah = 0x02
	outb	0x60
	call	kb_wait
	movb	al, 0xFF	! Pulse output port
	outb	0x64
	call    kb_wait		! Wait for the A20 line to settle down
	ret
kb_wait:
	inb	0x64
	testb	al, 0x02	! Keyboard input buffer full?
	jnz	kb_wait		! If so, wait
	ret

gate_PS_A20:		! The PS/2 can twiddle A20 using port A
	inb	0x92		! Read port A
	andb	al, 0xFD
	orb	al, ah		! Set A20 bit to the required state
	outb	0x92		! Write port A
	jmp	.+2		! Small delay
A20ok:	inb	0x92		! Check port A
	andb	al, 0x02
	cmpb	al, ah		! A20 line settled down to the new state?
	jne	A20ok		! If not then wait
	ret

! void int15(bios_env_t *ep)
!	Do an "INT 15" call, primarily for APM (Power Management).
.define _int15
_int15:
	push	si		! Save callee-save register si
	mov	si, sp
	mov	si, 4(si)	! ep
	mov	ax, (si)	! ep->ax
	mov	bx, 2(si)	! ep->bx
	mov	cx, 4(si)	! ep->cx
	int	0x15		! INT 0x15 BIOS call
	pushf			! Save flags
	mov	(si), ax	! ep->ax
	mov	2(si), bx	! ep->bx
	mov	4(si), cx	! ep->cx
	pop	6(si)		! ep->flags
	pop	si		! Restore
	ret

.sect	.rom
	.align	4
c60:	.data2	60		! Constants for MUL and DIV
c1024:	.data2	1024
c1080:	.data2	1080
c19663:	.data2	19663

.sect .data
	.align	4

! Global descriptor tables.
	UNSET	= 0		! Must be computed

! For "Extended Memory Block Move".
x_gdt:
x_null_desc:
	! Null descriptor
	.data2	0x0000, 0x0000
	.data1	0x00, 0x00, 0x00, 0x00
x_gdt_desc:
	! Descriptor for this descriptor table
	.data2	6*8-1, UNSET
	.data1	UNSET, 0x00, 0x00, 0x00
x_src_desc:
	! Source segment descriptor
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x00, 0x00
x_dst_desc:
	! Destination segment descriptor
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x00, 0x00
x_bios_desc:
	! BIOS segment descriptor (scratch for int 0x15)
	.data2	UNSET, UNSET
	.data1	UNSET, UNSET, UNSET, UNSET
x_ss_desc:
	! BIOS stack segment descriptor (scratch for int 0x15)
	.data2	UNSET, UNSET
	.data1	UNSET, UNSET, UNSET, UNSET

! Protected mode descriptor table.
p_gdt:
p_null_desc:
	! Null descriptor
	.data2	0x0000, 0x0000
	.data1	0x00, 0x00, 0x00, 0x00
p_gdt_desc:
	! Descriptor for this descriptor table
	.data2	8*8-1, UNSET
	.data1	UNSET, 0x00, 0x00, 0x00
p_idt_desc:
	! Real mode interrupt descriptor table descriptor
	.data2	0x03FF, 0x0000
	.data1	0x00, 0x00, 0x00, 0x00
p_ds_desc:
	! Kernel data segment descriptor (4Gb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0xCF, 0x00
p_es_desc:
	! Physical memory descriptor (4Gb flat)
	.data2	0xFFFF, 0x0000
	.data1	0x00, 0x92, 0xCF, 0x00
p_ss_desc:
	! Monitor data segment descriptor (64Kb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x00, 0x00
p_cs_desc:
	! Kernel code segment descriptor (4Gb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x9A, 0xCF, 0x00
p_mcs_desc:
	! Monitor code segment descriptor (64 kb flat) (unused)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x9A, 0x00, 0x00

xms_handle:	.data2	-1		! Handle of allocated XMS block
vfd:		.data2  -1		! Virtual disk file handle

.sect .bss
	.comm	xms_driver, 4	! Vector to XMS driver
	.comm	old_vid_mode, 2	! Video mode at startup
	.comm	cur_vid_mode, 2	! Current video mode
	.comm	msw, 4		! Saved machine status word (cr0)
	.comm	pdbr, 4		! Saved page directory base register (cr3)
	.comm	escape, 2	! Escape typed?
	.comm	bus, 2		! Saved return value of _get_bus
	.comm	unchar, 2	! Char returned by ungetch(c)

!
! $PchId: doshead.ack.s,v 1.7 2002/02/27 19:37:52 philip Exp $
