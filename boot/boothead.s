!	Boothead.s - BIOS support for boot.c		Author: Kees J. Bot
!
!
! This file contains the startup and low level support for the secondary
! boot program.  It contains functions for disk, tty and keyboard I/O,
! copying memory to arbitrary locations, etc.
!
! The primary bootstrap code supplies the following parameters in registers:
!	dl	= Boot-device.
!	es:si	= Partition table entry if hard disk.
!
.text

	o32	    =	  0x66	! This assembler doesn't know 386 extensions
	BOOTOFF	    =	0x7C00	! 0x0000:BOOTOFF load a bootstrap here
	LOADSEG     =	0x1000	! Where this code is loaded.
	BUFFER	    =	0x0600	! First free memory
	PENTRYSIZE  =	    16	! Partition table entry size.
	a_flags	    =	     2	! From a.out.h, struct exec
	a_text	    =	     8
	a_data	    =	    12
	a_bss	    =	    16
	a_total	    =	    24
	A_SEP	    =	  0x20	! Separate I&D flag
	K_I386	    =	0x0001	! Call Minix in 386 mode
	K_RET	    =	0x0020	! Returns to the monitor on reboot
	K_INT86	    =	0x0040	! Requires generic INT support
	K_MEML	    =	0x0080	! Pass a list of free memory

	DS_SELECTOR =	   3*8	! Kernel data selector
	ES_SELECTOR =	   4*8	! Flat 4 Gb
	SS_SELECTOR =	   5*8	! Monitor stack
	CS_SELECTOR =	   6*8	! Kernel code
	MCS_SELECTOR=	   7*8	! Monitor code

	ESC	    =	  0x1B	! Escape character

! Imported variables and functions:
.extern _caddr, _daddr, _runsize, _edata, _end	! Runtime environment
.extern _device					! BIOS device number
.extern _rem_part				! To pass partition info
.extern _k_flags				! Special kernel flags
.extern _mem					! Free memory list

.text

! Set segment registers and stack pointer using the programs own header!
! The header is either 32 bytes (short form) or 48 bytes (long form).  The
! bootblock will jump to address 0x10030 in both cases, calling one of the
! two jmpf instructions below.

	jmpf	boot, LOADSEG+3	! Set cs right (skipping long a.out header)
	.space	11		! jmpf + 11 = 16 bytes
	jmpf	boot, LOADSEG+2	! Set cs right (skipping short a.out header)
boot:
	mov	ax, #LOADSEG
	mov	ds, ax		! ds = header

	movb	al, a_flags
	testb	al, #A_SEP	! Separate I&D?
	jnz	sepID
comID:	xor	ax, ax
	xchg	ax, a_text	! No text
	add	a_data, ax	! Treat all text as data
sepID:
	mov	ax, a_total	! Total nontext memory usage
	and	ax, #0xFFFE	! Round down to even
	mov	a_total, ax	! total - text = data + bss + heap + stack
	cli			! Ignore interrupts while stack in limbo
	mov	sp, ax		! Set sp at the top of all that

	mov	ax, a_text	! Determine offset of ds above cs
	movb	cl, #4
	shr	ax, cl
	mov	cx, cs
	add	ax, cx
	mov	ds, ax		! ds = cs + text / 16
	mov	ss, ax
	sti			! Stack ok now
	push	es		! Save es, we need it for the partition table
	mov	es, ax
	cld			! C compiler wants UP

! Clear bss
	xor	ax, ax		! Zero
	mov	di, #_edata	! Start of bss is at end of data
	mov	cx, #_end	! End of bss (begin of heap)
	sub	cx, di		! Number of bss bytes
	shr	cx, #1		! Number of words
	rep
	stos			! Clear bss

! Copy primary boot parameters to variables.  (Can do this now that bss is
! cleared and may be written into).
	xorb	dh, dh
	mov	_device, dx	! Boot device (probably 0x00 or 0x80)
	mov	_rem_part+0, si	! Remote partition table offset
	pop	_rem_part+2	! and segment (saved es)

! Remember the current video mode for restoration on exit.
	movb	ah, #0x0F	! Get current video mode
	int	0x10
	andb	al, #0x7F	! Mask off bit 7 (no blanking)
	movb	old_vid_mode, al
	movb	cur_vid_mode, al

! Give C code access to the code segment, data segment and the size of this
! process.
	xor	ax, ax
	mov	dx, cs
	call	seg2abs
	mov	_caddr+0, ax
	mov	_caddr+2, dx
	xor	ax, ax
	mov	dx, ds
	call	seg2abs
	mov	_daddr+0, ax
	mov	_daddr+2, dx
	push	ds
	mov	ax, #LOADSEG
	mov	ds, ax		! Back to the header once more
	mov	ax, a_total+0
	mov	dx, a_total+2	! dx:ax = data + bss + heap + stack
	add	ax, a_text+0
	adc	dx, a_text+2	! dx:ax = text + data + bss + heap + stack
	pop	ds
	mov	_runsize+0, ax
	mov	_runsize+2, dx	! 32 bit size of this process

! Determine available memory as a list of (base,size) pairs as follows:
! mem[0] = low memory, mem[1] = memory between 1M and 16M, mem[2] = memory
! above 16M.  Last two coalesced into mem[1] if adjacent.
	mov	di, #_mem	! di = memory list
	int	0x12		! Returns low memory size (in K) in ax
	mul	c1024
	mov	4(di), ax	! mem[0].size = low memory size in bytes
	mov	6(di), dx
	call	_getprocessor
	cmp	ax, #286	! Only 286s and above have extended memory
	jb	no_ext
	cmp	ax, #486	! Assume 486s were the first to have >64M
	jb	small_ext	! (It helps to be paranoid when using the BIOS)
big_ext:
	mov	ax, #0xE801	! Code for get memory size for >64M
	int	0x15		! ax = mem at 1M per 1K, bx = mem at 16M per 64K
	jnc	got_ext
small_ext:
	movb	ah, #0x88	! Code for get extended memory size
	clc			! Carry will stay clear if call exists
	int	0x15		! Returns size (in K) in ax for AT's
	jc	no_ext
	test	ax, ax		! An AT with no extended memory?
	jz	no_ext
	xor	bx, bx		! bx = mem above 16M per 64K = 0
got_ext:
	mov	cx, ax		! cx = copy of ext mem at 1M
	mov	10(di), #0x0010	! mem[1].base = 0x00100000 (1M)
	mul	c1024
	mov	12(di), ax	! mem[1].size = "ext mem at 1M" * 1024
	mov	14(di), dx
	test	bx, bx
	jz	no_ext		! No more ext mem above 16M?
	cmp	cx, #15*1024	! Chunks adjacent? (precisely 15M at 1M?)
	je	adj_ext
	mov	18(di), #0x0100	! mem[2].base = 0x01000000 (16M)
	mov	22(di), bx	! mem[2].size = "ext mem at 16M" * 64K
	jmp	no_ext
adj_ext:
	add	14(di), bx	! Add ext mem above 16M to mem below 16M
no_ext:


! Time to switch to a higher level language (not much higher)
	call	_boot

! void ..exit(int status)
!	Exit the monitor by rebooting the system.
.define	_exit, __exit, ___exit		! Make various compilers happy
_exit:
__exit:
___exit:
	mov	bx, sp
	cmp	2(bx), #0		! Good exit status?
	jz	reboot
quit:	mov	ax, #any_key
	push	ax
	call	_printf
	xorb	ah, ah			! Read character from keyboard
	int	0x16
reboot:	call	dev_reset
	call	restore_video
	int	0x19			! Reboot the system
.data
any_key:
	.ascii	"\nHit any key to reboot\n\0"
.text

! u32_t mon2abs(void *ptr)
!	Address in monitor data to absolute address.
.define _mon2abs
_mon2abs:
	mov	bx, sp
	mov	ax, 2(bx)	! ptr
	mov	dx, ds		! Monitor data segment
	jmp	seg2abs

! u32_t vec2abs(vector *vec)
!	8086 interrupt vector to absolute address.
.define _vec2abs
_vec2abs:
	mov	bx, sp
	mov	bx, 2(bx)
	mov	ax, (bx)
	mov	dx, 2(bx)	! dx:ax vector
	!jmp	seg2abs		! Translate

seg2abs:			! Translate dx:ax to the 32 bit address dx-ax
	push	cx
	movb	ch, dh
	movb	cl, #4
	shl	dx, cl
	shrb	ch, cl		! ch-dx = dx << 4
	add	ax, dx
	adcb	ch, #0		! ch-ax = ch-dx + ax
	movb	dl, ch
	xorb	dh, dh		! dx-ax = ch-ax
	pop	cx
	ret

abs2seg:			! Translate the 32 bit address dx-ax to dx:ax
	push	cx
	movb	ch, dl
	mov	dx, ax		! ch-dx = dx-ax
	and	ax, #0x000F	! Offset in ax
	movb	cl, #4
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
	cmp	14(bp), #0
	jnz	bigcopy
	mov	cx, 12(bp)
	jcxz	copydone	! Count is zero, end copy
	cmp	cx, #0xFFF0
	jb	smallcopy
bigcopy:mov	cx, #0xFFF0	! Don't copy more than about 64K at once
smallcopy:
	push	cx		! Save copying count
	mov	ax, 4(bp)
	mov	dx, 6(bp)
	cmp	dx, #0x0010	! Copy to extended memory?
	jae	ext_copy
	cmp	10(bp), #0x0010	! Copy from extended memory?
	jae	ext_copy
	call	abs2seg
	mov	di, ax
	mov	es, dx		! es:di = dstaddr
	mov	ax, 8(bp)
	mov	dx, 10(bp)
	call	abs2seg
	mov	si, ax
	mov	ds, dx		! ds:si = srcaddr
	shr	cx, #1		! Words to move
	rep
	movs			! Do the word copy
	adc	cx, cx		! One more byte?
	rep
	movsb			! Do the byte copy
	mov	ax, ss		! Restore ds and es from the remaining ss
	mov	ds, ax
	mov	es, ax
	jmp	copyadjust
ext_copy:
	mov	x_dst_desc+2, ax
	movb	x_dst_desc+4, dl ! Set base of destination segment
	mov	ax, 8(bp)
	mov	dx, 10(bp)
	mov	x_src_desc+2, ax
	movb	x_src_desc+4, dl ! Set base of source segment
	mov	si, #x_gdt	! es:si = global descriptor table
	shr	cx, #1		! Words to move
	movb	ah, #0x87	! Code for extended memory move
	int	0x15
copyadjust:
	pop	cx		! Restore count
	add	4(bp), cx
	adc	6(bp), #0	! srcaddr += copycount
	add	8(bp), cx
	adc	10(bp), #0	! dstaddr += copycount
	sub	12(bp), cx
	sbb	14(bp), #0	! count -= copycount
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
	mov	ax, _caddr+0
	mov	dx, _caddr+2
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
	mov	_daddr+0, ax
	mov	_daddr+2, dx	! New data address
	push	cx		! New text segment
	push	bx		! Return offset of this function
	retf			! Relocate

! void *brk(void *addr)
! void *sbrk(size_t incr)
!	Cannot fail implementations of brk(2) and sbrk(3), so we can use
!	malloc(3).  They reboot on stack collision instead of returning -1.
.data
	.align	2
break:	.data2	_end		! A fake heap pointer
.text
.define _brk, __brk, _sbrk, __sbrk
_brk:
__brk:				! __brk is for the standard C compiler
	xor	ax, ax
	jmp	sbrk		! break= 0; return sbrk(addr);
_sbrk:
__sbrk:
	mov	ax, break	! ax= current break
sbrk:	push	ax		! save it as future return value
	mov	bx, sp		! Stack is now: (retval, retaddr, incr, ...)
	add	ax, 4(bx)	! ax= break + increment
	mov	break, ax	! Set new break
	lea	dx, -1024(bx)	! sp minus a bit of breathing space
	cmp	dx, ax		! Compare with the new break
	jb	heaperr		! Suffocating noises
	lea	dx, -4096(bx)	! A warning when heap+stack goes < 4K
	cmp	dx, ax
	jae	plenty		! No reason to complain
	mov	ax, #memwarn
	push	ax
	call	_printf		! Warn about memory running low
	pop	ax
	movb	memwarn, #0	! No more warnings
plenty:	pop	ax		! Return old break (0 for brk)
	ret
heaperr:mov	ax, #chmem
	push	ax
	mov	ax, #nomem
	push	ax
	call	_printf
	jmp	quit
.data
nomem:	.ascii	"\nOut of%s\0"
memwarn:.ascii	"\nLow on"
chmem:	.ascii	" memory, use chmem to increase the heap\n\0"
.text

! int dev_open(void);
!	Given the device "_device" figure out if it exists and what its number
!	of heads and sectors may be.  Return the BIOS error code on error,
!	otherwise 0.
.define	_dev_open
_dev_open:
	call	dev_reset	! Optionally reset the disks
	movb	dev_state, #0	! State is "closed"
	push	es
	push	di		! Save registers used by BIOS calls
	movb	dl, _device	! The default device
	cmpb	dl, #0x80	! Floppy < 0x80, winchester >= 0x80
	jae	winchester
floppy:
	mov	di, #3		! Three tries to init drive by reading sector 0
finit0:	xor	ax, ax
	mov	es, ax
	mov	bx, #BUFFER	! es:bx = scratch buffer
	mov	ax, #0x0201	! Read sector, #sectors = 1
	mov	cx, #0x0001	! Track 0, first sector
	xorb	dh, dh		! Drive dl, head 0
	int	0x13
	jnc	finit0ok	! Sector 0 read ok?
	cmpb	ah, #0x80	! Disk timed out?  (Floppy drive empty)
	je	geoerr
	dec	di
	jz	geoerr
	xorb	ah, ah		! Reset drive
	int	0x13
	jc	geoerr
	jmp	finit0		! Retry once more, it may need to spin up
finit0ok:
	mov	di, #seclist	! List of per floppy type sectors/track
flast:	movb	cl, (di)	! Sectors per track to test
	cmpb	cl, #9		! No need to do the last 720K/360K test
	je	ftestok
	xor	ax, ax
	mov	es, ax
	mov	bx, #BUFFER	! es:bx = scratch buffer
	mov	ax, #0x0201	! Read sector, #sectors = 1
	xorb	ch, ch		! Track 0, last sector
	xorb	dh, dh		! Drive dl, head 0
	int	0x13
	jnc	ftestok		! Sector cl read ok?
	xorb	ah, ah		! Reset drive
	int	0x13
	jc	geoerr
	inc	di		! Try next sec/track number
	jmp	flast
ftestok:
	movb	dh, #2		! Floppies have two sides
	jmp	geoboth
winchester:
	movb	ah, #0x08	! Code for drive parameters
	int	0x13		! dl still contains drive
	jc	geoerr		! No such drive?
	andb	cl, #0x3F	! cl = max sector number (1-origin)
	incb	dh		! dh = 1 + max head number (0-origin)
geoboth:
	movb	sectors, cl	! Sectors per track
	movb	al, cl		! al = sectors per track
	mulb	dh		! ax = heads * sectors
	mov	secspcyl, ax	! Sectors per cylinder = heads * sectors
	movb	dev_state, #1	! Device state is "open"
	xor	ax, ax		! Code for success
geodone:
	pop	di
	pop	es		! Restore di and es registers
	ret
geoerr:	movb	al, ah
	xorb	ah, ah		! ax = BIOS error code
	jmp	geodone
.data
seclist:
	.data1	18, 15, 9	! 1.44M, 1.2M, and 360K/720K floppy sec/track
.text

! int dev_close(void);
!	Close the current device.  Under the BIOS this does nothing much.
.define	_dev_close
_dev_close:
	xor	ax, ax
	movb	dev_state, al	! State is "closed"
	ret

! Reset the disks if needed.  Minix may have messed things up.
dev_reset:
	cmpb	dev_state, #0	! Need reset if dev_state < 0
	jge	0f
	xorb	ah, ah		! Reset (ah = 0)
	movb	dl, #0x80	! All disks
	int	0x13
	movb	dev_state, #0	! State is "closed"
0:	ret

! int dev_boundary(u32_t sector);
!	True if a sector is on a boundary, i.e. sector % sectors == 0.
.define	_dev_boundary
_dev_boundary:
	mov	bx, sp
	xor	dx, dx
	mov	ax, 4(bx)	! divide high half of sector number
	div	sectors
	mov	ax, 2(bx)	! divide low half of sector number
	div	sectors		! dx = sector % sectors
	sub	dx, #1		! CF = dx == 0
	sbb	ax, ax		! ax = -CF
	neg	ax		! ax = (sector % sectors) == 0
	ret

! int readsectors(u32_t bufaddr, u32_t sector, u8_t count)
! int writesectors(u32_t bufaddr, u32_t sector, u8_t count)
!	Read/write several sectors from/to disk or floppy.  The buffer must
!	be between 64K boundaries!  Count must fit in a byte.  The external
!	variables _device, sectors and secspcyl describe the disk and its
!	geometry.  Returns 0 for success, otherwise the BIOS error code.
!
.define _readsectors, _writesectors
_writesectors:
	push	bp
	mov	bp, sp
	movb	13(bp), #0x03	! Code for a disk write
	jmp	rwsec
_readsectors:
	push	bp
	mov	bp, sp
	movb	13(bp), #0x02	! Code for a disk read
rwsec:	push	si
	push	di
	push	es
	cmpb	dev_state, #0	! Device state?
	jg	0f		! >0 if open
	call	_dev_open	! Initialize
	test	ax, ax
	jnz	badopen
0:	mov	ax, 4(bp)
	mov	dx, 6(bp)
	call	abs2seg
	mov	bx, ax
	mov	es, dx		! es:bx = bufaddr
	mov	di, #3		! Execute 3 resets on floppy error
	cmpb	_device, #0x80
	jb	nohd
	mov	di, #1		! But only 1 reset on hard disk error
nohd:	cmpb	12(bp), #0	! count equals zero?
	jz	done
more:	mov	ax, 8(bp)
	mov	dx, 10(bp)	! dx:ax = abs sector.  Divide it by sectors/cyl
	cmp	dx, #[1024*255*63-255]>>16  ! Near 8G limit?
	jae	bigdisk
	div	secspcyl	! ax = cylinder, dx = sector within cylinder
	xchg	ax, dx		! ax = sector within cylinder, dx = cylinder
	movb	ch, dl		! ch = low 8 bits of cylinder
	divb	sectors		! al = head, ah = sector (0-origin)
	xorb	dl, dl		! About to shift bits 8-9 of cylinder into dl
	shr	dx, #1
	shr	dx, #1		! dl[6..7] = high cylinder
	orb	dl, ah		! dl[0..5] = sector (0-origin)
	movb	cl, dl		! cl[0..5] = sector, cl[6..7] = high cyl
	incb	cl		! cl[0..5] = sector (1-origin)
	movb	dh, al		! dh = head
	movb	dl, _device	! dl = device to use
	movb	al, sectors	! Sectors per track - Sector number (0-origin)
	subb	al, ah		! = Sectors left on this track
	cmpb	al, 12(bp)	! Compare with # sectors to transfer
	jbe	doit		! Can't go past the end of a cylinder?
	movb	al, 12(bp)	! 12(bp) < sectors left on this track
doit:	movb	ah, 13(bp)	! Code for disk read (0x02) or write (0x03)
	push	ax		! Save al = sectors to read
	int	0x13		! call the BIOS to do the transfer
	pop	cx		! Restore al in cl
	jmp	rdeval
bigdisk:
	mov	si, #ext_rw	! si = extended read/write parameter packet
	movb	cl, 12(bp)
	movb	2(si), cl	! Fill in # blocks to transfer
	mov	4(si), bx	! Buffer address = es:bx
	mov	6(si), es
	mov	8(si), ax	! Starting block number = dx:ax
	mov	10(si), dx
	movb	dl, _device	! dl = device to use
	mov	ax, #0x4000	! This, or-ed with 0x02 or 0x03 becomes
	orb	ah, 13(bp)	! extended read (0x4200) or write (0x4300)
	int	0x13
	!jmp	rdeval
rdeval:
	jc	ioerr		! I/O error
	movb	al, cl		! Restore al = sectors read
	addb	bh, al		! bx += 2 * al * 256 (add bytes transferred)
	addb	bh, al		! es:bx = where next sector is located
	add	8(bp), ax	! Update address by sectors transferred
	adc	10(bp), #0	! Don't forget high word
	subb	12(bp), al	! Decrement sector count by sectors transferred
	jnz	more		! Not all sectors have been transferred
done:	xorb	ah, ah		! No error here!
	jmp	finish
ioerr:	cmpb	ah, #0x80	! Disk timed out?  (Floppy drive empty)
	je	finish
	cmpb	ah, #0x03	! Disk write protected?
	je	finish
	dec	di		! Do we allow another reset?
	jl	finish		! No, report the error
	xorb	ah, ah		! Code for a reset (0)
	int	0x13
	jnc	more		! Succesful reset, try request again
finish:	movb	al, ah
	xorb	ah, ah		! ax = error number
badopen:pop	es
	pop	di
	pop	si
	pop	bp
	ret
.data
	.align	4
! Extended read/write commands require a parameter packet.
ext_rw:
	.data1	0x10		! Length of extended r/w packet
	.data1	0		! Reserved
	.data2	0		! Blocks to transfer (to be filled in)
	.data2	0		! Buffer address offset (tbfi)
	.data2	0		! Buffer address segment (tbfi)
	.data4	0		! Starting block number low 32 bits (tbfi)
	.data4	0		! Starting block number high 32 bits
.text

! int getch(void);
!	Read a character from the keyboard, and check for an expired timer.
!	A carriage return is changed into a linefeed for UNIX compatibility.
.define _getch
_getch:
	xor	ax, ax
	xchg	ax, unchar	! Ungotten character?
	test	ax, ax
	jnz	gotch
getch:
	hlt			! Play dead until interrupted (see pause())
	movb	ah, #0x01	! Keyboard status
	int	0x16
	jz	0f		! Nothing typed
	xorb	ah, ah		! Read character from keyboard
	int	0x16
	jmp	press		! Keypress
0:	mov	dx, line	! Serial line?
	test	dx, dx
	jz	0f
	add	dx, #5		! Line Status Register
	inb	dx
	testb	al, #0x01	! Data Ready?
	jz	0f
	mov	dx, line
	!add	dx, 0		! Receive Buffer Register
	inb	dx		! Get character
	jmp	press
0:	call	_expired	! Timer expired?
	test	ax, ax
	jz	getch
	mov	ax, #ESC	! Return ESC
	ret
press:
	cmpb	al, #0x0D	! Carriage return?
	jnz	nocr
	movb	al, #0x0A	! Change to linefeed
nocr:	cmpb	al, #ESC	! Escape typed?
	jne	noesc
	inc	escape		! Set flag
noesc:	xorb	ah, ah		! ax = al
gotch:	ret

! int ungetch(void);
!	Return a character to undo a getch().
.define _ungetch
_ungetch:
	mov	bx, sp
	mov	ax, 2(bx)
	mov	unchar, ax
	ret

! int escape(void);
!	True if ESC has been typed.
.define _escape
_escape:
	movb	ah, #0x01	! Keyboard status
	int	0x16
	jz	escflg		! Keypress?
	cmpb	al, #ESC	! Escape typed?
	jne	escflg
	xorb	ah, ah		! Discard the escape
	int	0x16
	inc	escape		! Set flag
escflg:	xor	ax, ax
	xchg	ax, escape	! Escape typed flag
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
	cmpb	al, #0x0A	! al = newline?
	jnz	putc
	movb	al, #0x0D
	call	putc		! putc('\r')
	movb	al, #0x0A	! Restore the '\n' and print it
putc:	movb	ah, #0x0E	! Print character in teletype mode
	mov	bx, #0x0001	! Page 0, foreground color
	int	0x10
	mov	bx, line	! Serial line?
	test	bx, bx
	jz	nulch
	push	ax		! Save character to print
	call	_get_tick	! Current clock tick counter
	mov	cx, ax
	add	cx, #2		! Don't want to see it count twice
1:	lea	dx, 5(bx)	! Line Status Register
	inb	dx
	testb	al, #0x20	! Transmitter Holding Register Empty?
	jnz	0f
	call	_get_tick
	cmp	ax, cx		! Clock ticked more than once?
	jne	1b
0:	pop	ax		! Restore character to print
	mov	dx, bx		! Transmit Holding Register
	outb	dx		! Send character down the serial line
nulch:	ret

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
	cmp	ax, cur_vid_mode
	je	modeok		! Mode already as requested?
	mov	cur_vid_mode, ax
_clear_screen:
	xor	ax, ax
	mov	es, ax		! es = Vector segment
	mov	ax, cur_vid_mode
	movb	ch, ah		! Copy of the special flags
	andb	ah, #0x0F	! Test bits 8-11, clear special flags
	jnz	xvesa		! VESA extended mode?
	int	0x10		! Reset video (ah = 0)
	jmp	md_480
xvesa:	mov	bx, ax		! bx = extended mode
	mov	ax, #0x4F02	! Reset video
	int	0x10
md_480:				! Basic video mode is set, now build on it
	testb	ch, #0x20	! 480 scan lines requested?
	jz	md_14pt
	mov	dx, #0x3CC	! Get CRTC port
	inb	dx
	movb	dl, #0xD4
	testb	al, #1		! Mono or color?
	jnz	0f
	movb	dl, #0xB4
0:	mov	ax, #0x110C	! Vertical sync end (also unlocks CR0-7)
	call	out2
	mov	ax, #0x060B	! Vertical total
	call	out2
	mov	ax, #0x073E	! (Vertical) overflow
	call	out2
	mov	ax, #0x10EA	! Vertical sync start
	call	out2
	mov	ax, #0x12DF	! Vertical display end
	call	out2
	mov	ax, #0x15E7	! Vertical blank start
	call	out2
	mov	ax, #0x1604	! Vertical blank end
	call	out2
	push	dx
	movb	dl, #0xCC	! Misc output register (read)
	inb	dx
	movb	dl, #0xC2	! (write)
	andb	al, #0x0D	! Preserve clock select bits and color bit
	orb	al, #0xE2	! Set correct sync polarity
	outb	dx
	pop	dx		! Index register still in dx
md_14pt:
	testb	ch, #0x40	! 9x14 point font requested?
	jz	md_8pt
	mov	ax, #0x1111	! Load ROM 9 by 14 font
	xorb	bl, bl		! Load block 0
	int	0x10
	testb	ch, #0x20	! 480 scan lines?
	jz	md_8pt
	mov	ax, #0x12DB	! VGA vertical display end
	call	out2
   eseg	movb	0x0484, #33	! Tell BIOS the last line number
md_8pt:
	testb	ch, #0x80	! 8x8 point font requested?
	jz	setcur
	mov	ax, #0x1112	! Load ROM 8 by 8 font
	xorb	bl, bl		! Load block 0
	int	0x10
	testb	ch, #0x20	! 480 scan lines?
	jz	setcur
	mov	ax, #0x12DF	! VGA vertical display end
	call	out2
   eseg	movb	0x0484, #59	! Tell BIOS the last line number
setcur:
	xor	dx, dx		! dl = column = 0, dh = row = 0
	xorb	bh, bh		! Page 0
	movb	ah, #0x02	! Set cursor position
	int	0x10
	push	ss
	pop	es		! Restore es
modeok:	ret

! Out to the usual [index, data] port pair that are common for VGA devices
! dx = port, ah = index, al = data.
out2:
	push	dx
	push	ax
	movb	al, ah
	outb	dx		! Set index
	inc	dx
	pop	ax
	outb	dx		! Send data
	pop	dx
	ret

restore_video:			! To restore the video mode on exit
	mov	ax, old_vid_mode
	push	ax
	call	_set_mode
	pop	ax
	ret

! void serial_init(int line)
!	Initialize copying console I/O to a serial line.
.define	_serial_init
_serial_init:
	mov	bx, sp
	mov	dx, 2(bx)	! Line number
	push	ds
	xor	ax, ax
	mov	ds, ax		! Vector and BIOS data segment
	mov	bx, dx		! Line number
	shl	bx, #1		! Word offset
	mov	bx, 0x0400(bx)	! I/O port for the given line
	pop	ds
	mov	line, bx	! Remember I/O port
serial_init:
	mov	bx, line
	test	bx, bx		! I/O port must be nonzero
	jz	0f
	mov	ax, #0x00E3	! 9600 N-8-1
	int	0x14		! Initialize serial line dx
	lea	dx, 4(bx)	! Modem Control Register
	movb	al, #0x0B	! DTR, RTS, OUT2
	outb	dx
0:	ret

! u32_t get_tick(void);
!	Return the current value of the clock tick counter.  This counter
!	increments 18.2 times per second.  Poll it to do delays.  Does not
!	work on the original PC, but works on the PC/XT.
.define _get_tick
_get_tick:
	push	cx
	xorb	ah, ah		! Code for get tick count
	int	0x1A
	mov	ax, dx
	mov	dx, cx		! dx:ax = cx:dx = tick count
	pop	cx
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
	call	_getprocessor
	xor	dx, dx		! Assume XT
	cmp	ax, #286	! An AT has at least a 286
	jb	got_bus
	inc	dx		! Assume AT
	movb	ah, #0xC0	! Code for get configuration
	int	0x15
	jc	got_bus		! Carry clear and ah = 00 if supported
	testb	ah, ah
	jne	got_bus
	eseg
	movb	al, 5(bx)	! Load feature byte #1
	inc	dx		! Assume MCA
	testb	al, #0x02	! Test bit 1 - "bus is Micro Channel"
	jnz	got_bus
	dec	dx		! Assume AT
	testb	al, #0x40	! Test bit 6 - "2nd 8259 installed"
	jnz	got_bus
	dec	dx		! It is an XT
got_bus:
	push	ds
	pop	es		! Restore es
	mov	ax, dx		! Return bus code
	mov	bus, ax		! Keep bus code, A20 handler likes to know
	ret

! u16_t get_video(void)
!	Return type of display, in order: MDA, CGA, mono EGA, color EGA,
!	mono VGA, color VGA.
_get_video:
	mov	ax, #0x1A00	! Function 1A returns display code
	int	0x10		! al = 1A if supported
	cmpb	al, #0x1A
	jnz	no_dc		! No display code function supported

	mov	ax, #2
	cmpb	bl, #5		! Is it a monochrome EGA?
	jz	got_video
	inc	ax
	cmpb	bl, #4		! Is it a color EGA?
	jz	got_video
	inc	ax
	cmpb	bl, #7		! Is it a monochrome VGA?
	jz	got_video
	inc	ax
	cmpb	bl, #8		! Is it a color VGA?
	jz	got_video

no_dc:	movb	ah, #0x12	! Get information about the EGA
	movb	bl, #0x10
	int	0x10
	cmpb	bl, #0x10	! Did it come back as 0x10? (No EGA)
	jz	no_ega

	mov	ax, #2
	cmpb	bh, #1		! Is it monochrome?
	jz	got_video
	inc	ax
	jmp	got_video

no_ega:	int	0x11		! Get bit pattern for equipment
	and	ax, #0x30	! Isolate color/mono field
	sub	ax, #0x30
	jz	got_video	! Is it an MDA?
	mov	ax, #1		! No it's CGA

got_video:
	ret


! Functions to leave the boot monitor.
.define _bootstrap		! Call another bootstrap
.define _minix			! Call Minix

! void _bootstrap(int device, struct part_entry *entry)
!	Call another bootstrap routine to boot MS-DOS for instance.  (No real
!	need for that anymore, now that you can format floppies under Minix).
!	The bootstrap must have been loaded at BOOTSEG from "device".
_bootstrap:
	call	restore_video
	mov	bx, sp
	movb	dl, 2(bx)	! Device to boot from
	mov	si, 4(bx)	! ds:si = partition table entry
	xor	ax, ax
	mov	es, ax		! Vector segment
	mov	di, #BUFFER	! es:di = buffer in low core
	mov	cx, #PENTRYSIZE	! cx = size of partition table entry
 rep	movsb			! Copy the entry to low core
	mov	si, #BUFFER	! es:si = partition table entry
	mov	ds, ax		! Some bootstraps need zero segment registers
	cli
	mov	ss, ax
	mov	sp, #BOOTOFF	! This should do it
	sti
	jmpf	BOOTOFF, 0	! Back to where the BIOS loads the boot code

! void minix(u32_t koff, u32_t kcs, u32_t kds,
!				char *bootparams, size_t paramsize, u32_t aout);
!	Call Minix.
_minix:
	push	bp
	mov	bp, sp		! Pointer to arguments

	mov	dx, #0x03F2	! Floppy motor drive control bits
	movb	al, #0x0C	! Bits 4-7 for floppy 0-3 are off
	outb	dx		! Kill the motors
	push	ds
	xor	ax, ax		! Vector & BIOS data segments
	mov	ds, ax
	andb	0x043F, #0xF0	! Clear diskette motor status bits of BIOS
	pop	ds
	cli			! No more interruptions

	test	_k_flags, #K_I386 ! Switch to 386 mode?
	jnz	minix386

! Call Minix in real mode.
minix86:
	test	_k_flags, #K_MEML ! New memory arrangements?
	jz	0f
	push	22(bp)		! Address of a.out headers
	push	20(bp)
0:
	push	18(bp)		! # bytes of boot parameters
	push	16(bp)		! Address of boot parameters

	test	_k_flags, #K_RET ! Can the kernel return?
	jz	noret86
	xor	dx, dx		! If little ext mem then monitor not preserved
	xor	ax, ax
	cmp	_mon_return, ax	! Minix can return to the monitor?
	jz	0f
	mov	dx, cs		! Monitor far return address
	mov	ax, #ret86
0:	push	dx		! Push monitor far return address or zero
	push	ax
noret86:

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

! Call Minix in 386 mode.
minix386:
  cseg	mov	cs_real-2, cs	! Patch CS and DS into the instructions that
  cseg	mov	ds_real-2, ds	! reload them when switching back to real mode
	.data1	0x0F,0x20,0xC0	! mov	eax, cr0
	orb	al, #0x01	! Set PE (protection enable) bit
	.data1	o32
	mov	msw, ax		! Save as protected mode machine status word

	mov	dx, ds		! Monitor ds
	mov	ax, #p_gdt	! dx:ax = Global descriptor table
	call	seg2abs
	mov	p_gdt_desc+2, ax
	movb	p_gdt_desc+4, dl ! Set base of global descriptor table

	mov	ax, 12(bp)
	mov	dx, 14(bp)	! Kernel ds (absolute address)
	mov	p_ds_desc+2, ax
	movb	p_ds_desc+4, dl ! Set base of kernel data segment

	mov	dx, ss		! Monitor ss
	xor	ax, ax		! dx:ax = Monitor stack segment
	call	seg2abs		! Minix starts with the stack of the monitor
	mov	p_ss_desc+2, ax
	movb	p_ss_desc+4, dl

	mov	ax, 8(bp)
	mov	dx, 10(bp)	! Kernel cs (absolute address)
	mov	p_cs_desc+2, ax
	movb	p_cs_desc+4, dl

	mov	dx, cs		! Monitor cs
	xor	ax, ax		! dx:ax = Monitor code segment
	call	seg2abs
	mov	p_mcs_desc+2, ax
	movb	p_mcs_desc+4, dl

	push	#MCS_SELECTOR
	test	_k_flags, #K_INT86 ! Generic INT86 support?
	jz	0f
	push	#int86		! Far address to INT86 support
	jmp	1f
0:	push	#bios13		! Far address to BIOS int 13 support
1:
	test	_k_flags, #K_MEML ! New memory arrangements?
	jz	0f
	.data1	o32
	push	20(bp)		! Address of a.out headers
0:
	push	#0
	push	18(bp)		! 32 bit size of parameters on stack
	push	#0
	push	16(bp)		! 32 bit address of parameters (ss relative)

	test	_k_flags, #K_RET ! Can the kernel return?
	jz	noret386
	push	#MCS_SELECTOR
	push	#ret386		! Monitor far return address
noret386:

	push	#0
	push	#CS_SELECTOR
	push	6(bp)
	push	4(bp)		! 32 bit far address to kernel entry point

	call	real2prot	! Switch to protected mode
	mov	ax, #DS_SELECTOR ! Kernel data
	mov	ds, ax
	mov	ax, #ES_SELECTOR ! Flat 4 Gb
	mov	es, ax
	.data1	o32		! Make a far call to the kernel
	retf

! Minix-86 returns here on a halt or reboot.
ret86:
	mov	_reboot_code+0, ax
	mov	_reboot_code+2, dx	! Return value (obsolete method)
	jmp	return

! Minix-386 returns here on a halt or reboot.
ret386:
	.data1	o32
	mov	_reboot_code, ax	! Return value (obsolete method)
	call	prot2real	! Switch to real mode

return:
	mov	sp, bp		! Pop parameters
	sti			! Can take interrupts again

	call	_get_video	! MDA, CGA, EGA, ...
	movb	dh, #24		! dh = row 24
	cmp	ax, #2		! At least EGA?
	jb	is25		! Otherwise 25 rows
	push	ds
	xor	ax, ax		! Vector & BIOS data segments
	mov	ds, ax
	movb	dh, 0x0484	! Number of rows on display minus one
	pop	ds
is25:
	xorb	dl, dl		! dl = column 0
	xorb	bh, bh		! Page 0
	movb	ah, #0x02	! Set cursor position
	int	0x10

	movb	dev_state, #-1	! Minix may have upset the disks, must reset.
	call	serial_init	! Likewise with our serial console

	call	_getprocessor
	cmp	ax, #286
	jb	noclock
	xorb	al, al
tryclk:	decb	al
	jz	noclock
	movb	ah, #0x02	! Get real-time clock time (from CMOS clock)
	int	0x1A
	jc	tryclk		! Carry set, not running or being updated
	movb	al, ch		! ch = hour in BCD
	call	bcd		! al = (al >> 4) * 10 + (al & 0x0F)
	mulb	c60		! 60 minutes in an hour
	mov	bx, ax		! bx = hour * 60
	movb	al, cl		! cl = minutes in BCD
	call	bcd
	add	bx, ax		! bx = hour * 60 + minutes
	movb	al, dh		! dh = seconds in BCD
	call	bcd
	xchg	ax, bx		! ax = hour * 60 + minutes, bx = seconds
	mul	c60		! dx-ax = (hour * 60 + minutes) * 60
	add	bx, ax
	adc	dx, #0		! dx-bx = seconds since midnight
	mov	ax, dx
	mul	c19663
	xchg	ax, bx
	mul	c19663
	add	dx, bx		! dx-ax = dx-bx * (0x1800B0 / (2*2*2*2*5))
	mov	cx, ax		! (0x1800B0 = ticks per day of BIOS clock)
	mov	ax, dx
	xor	dx, dx
	div	c1080
	xchg	ax, cx
	div	c1080		! cx-ax = dx-ax / (24*60*60 / (2*2*2*2*5))
	mov	dx, ax		! cx-dx = ticks since midnight
	movb	ah, #0x01	! Set system time
	int	0x1A
noclock:

	pop	bp
	ret			! Return to monitor as if nothing much happened

! Transform BCD number in al to a regular value in ax.
bcd:	movb	ah, al
	shrb	ah, #4
	andb	al, #0x0F
	.data1 0xD5,10 ! aad	! ax = (al >> 4) * 10 + (al & 0x0F)
	ret			! (BUG: assembler messes up aad & aam!)

! Support function for Minix-386 to make a BIOS int 13 call (disk I/O).
bios13:
	mov	bp, sp
	call	prot2real
	sti			! Enable interrupts

	mov	ax, 8(bp)	! Load parameters
	mov	bx, 10(bp)
	mov	cx, 12(bp)
	mov	dx, 14(bp)
	mov	es, 16(bp)
	int	0x13		! Make the BIOS call
	mov	8(bp), ax	! Save results
	mov	10(bp), bx
	mov	12(bp), cx
	mov	14(bp), dx
	mov	16(bp), es

	cli			! Disable interrupts
	call	real2prot
	mov	ax, #DS_SELECTOR ! Kernel data
	mov	ds, ax
	.data1	o32
	retf			! Return to the kernel

! Support function for Minix-386 to make an 8086 interrupt call.
int86:
	mov	bp, sp
	call	prot2real

	.data1	o32
	xor	ax, ax
	mov	es, ax		! Vector & BIOS data segments
	.data1	o32
   eseg	mov	0x046C, ax	! Clear BIOS clock tick counter

	sti			! Enable interrupts

	movb	al, #0xCD	! INT instruction
	movb	ah, 8(bp)	! Interrupt number?
	testb	ah, ah
	jnz	0f		! Nonzero if INT, otherwise far call
	push	cs
	push	#intret+2	! Far return address
	.data1	o32
	push	12(bp)		! Far driver address
	mov	ax, #0x90CB	! RETF; NOP
0:
 cseg	cmp	ax, intret	! Needs to be changed?
	je	0f		! If not then avoid a huge I-cache stall
   cseg	mov	intret, ax	! Patch 'INT n' or 'RETF; NOP' into code
	jmp	.+2		! Clear instruction queue
0:
	mov	ds, 16(bp)	! Load parameters
	mov	es, 18(bp)
	.data1	o32
	mov	ax, 20(bp)
	.data1	o32
	mov	bx, 24(bp)
	.data1	o32
	mov	cx, 28(bp)
	.data1	o32
	mov	dx, 32(bp)
	.data1	o32
	mov	si, 36(bp)
	.data1	o32
	mov	di, 40(bp)
	.data1	o32
	mov	bp, 44(bp)

intret:	int	0xFF		! Do the interrupt or far call

	.data1	o32		! Save results
	push	bp
	.data1	o32
	pushf
	mov	bp, sp
	.data1	o32
	pop	8+8(bp)		! eflags
	mov	8+16(bp), ds
	mov	8+18(bp), es
	.data1	o32
	mov	8+20(bp), ax
	.data1	o32
	mov	8+24(bp), bx
	.data1	o32
	mov	8+28(bp), cx
	.data1	o32
	mov	8+32(bp), dx
	.data1	o32
	mov	8+36(bp), si
	.data1	o32
	mov	8+40(bp), di
	.data1	o32
	pop	8+44(bp)	! ebp

	cli			! Disable interrupts

	xor	ax, ax
	mov	ds, ax		! Vector & BIOS data segments
	.data1	o32
	mov	cx, 0x046C	! Collect lost clock ticks in ecx

	mov	ax, ss
	mov	ds, ax		! Restore monitor ds
	call	real2prot
	mov	ax, #DS_SELECTOR ! Kernel data
	mov	ds, ax
	.data1	o32
	retf			! Return to the kernel

! Switch from real to protected mode.
real2prot:
	movb	ah, #0x02	! Code for A20 enable
	call	gate_A20

	lgdt	p_gdt_desc	! Global descriptor table
	.data1	o32
	mov	ax, pdbr	! Load page directory base register
	.data1	0x0F,0x22,0xD8	! mov	cr3, eax
	.data1	0x0F,0x20,0xC0	! mov	eax, cr0
	.data1	o32
	xchg	ax, msw		! Exchange real mode msw for protected mode msw
	.data1	0x0F,0x22,0xC0	! mov	cr0, eax
	jmpf	cs_prot, MCS_SELECTOR ! Set code segment selector
cs_prot:
	mov	ax, #SS_SELECTOR ! Set data selectors
	mov	ds, ax
	mov	es, ax
	mov	ss, ax
	ret

! Switch from protected to real mode.
prot2real:
	lidt	p_idt_desc	! Real mode interrupt vectors
	.data1	0x0F,0x20,0xD8	! mov	eax, cr3
	.data1	o32
	mov	pdbr, ax	! Save page directory base register
	.data1	0x0F,0x20,0xC0	! mov	eax, cr0
	.data1	o32
	xchg	ax, msw		! Exchange protected mode msw for real mode msw
	.data1	0x0F,0x22,0xC0	! mov	cr0, eax
	jmpf	cs_real, 0xDEAD	! Reload cs register
cs_real:
	mov	ax, #0xBEEF
ds_real:
	mov	ds, ax		! Reload data segment registers
	mov	es, ax
	mov	ss, ax

	xorb	ah, ah		! Code for A20 disable
	!jmp	gate_A20

! Enable (ah = 0x02) or disable (ah = 0x00) the A20 address line.
gate_A20:
	cmp	bus, #2		! PS/2 bus?
	je	gate_PS_A20
	call	kb_wait
	movb	al, #0xD1	! Tell keyboard that a command is coming
	outb	0x64
	call	kb_wait
	movb	al, #0xDD	! 0xDD = A20 disable code if ah = 0x00
	orb	al, ah		! 0xDF = A20 enable code if ah = 0x02
	outb	0x60
	call	kb_wait
	movb	al, #0xFF	! Pulse output port
	outb	0x64
	call	kb_wait		! Wait for the A20 line to settle down
	ret
kb_wait:
	inb	0x64
	testb	al, #0x02	! Keyboard input buffer full?
	jnz	kb_wait		! If so, wait
	ret

gate_PS_A20:		! The PS/2 can twiddle A20 using port A
	inb	0x92		! Read port A
	andb	al, #0xFD
	orb	al, ah		! Set A20 bit to the required state
	outb	0x92		! Write port A
	jmp	.+2		! Small delay
A20ok:	inb	0x92		! Check port A
	andb	al, #0x02
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

! void scan_keyboard(void)
!	Read keyboard character. Needs to be done in case one is waiting.
.define _scan_keyboard
_scan_keyboard:
	inb	0x60
	inb	0x61
	movb	ah, al
	orb	al, #0x80
	outb	0x61
	movb	al, ah
	outb	0x61
	ret

.data
	.ascii	"(null)\0"	! Just in case someone follows a null pointer
	.align	2
c60:	.data2	60		! Constants for MUL and DIV
c1024:	.data2	1024
c1080:	.data2	1080
c19663:	.data2	19663

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
	! Kernel data segment descriptor (4 Gb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0xCF, 0x00
p_es_desc:
	! Physical memory descriptor (4 Gb flat)
	.data2	0xFFFF, 0x0000
	.data1	0x00, 0x92, 0xCF, 0x00
p_ss_desc:
	! Monitor data segment descriptor (64 kb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x92, 0x00, 0x00
p_cs_desc:
	! Kernel code segment descriptor (4 Gb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x9A, 0xCF, 0x00
p_mcs_desc:
	! Monitor code segment descriptor (64 kb flat)
	.data2	0xFFFF, UNSET
	.data1	UNSET, 0x9A, 0x00, 0x00

.bss
	.comm	old_vid_mode, 2	! Video mode at startup
	.comm	cur_vid_mode, 2	! Current video mode
	.comm	dev_state, 2	! Device state: reset (-1), closed (0), open (1)
	.comm	sectors, 2	! # sectors of current device
	.comm	secspcyl, 2	! (Sectors * heads) of current device
	.comm	msw, 4		! Saved machine status word (cr0)
	.comm	pdbr, 4		! Saved page directory base register (cr3)
	.comm	escape, 2	! Escape typed?
	.comm	bus, 2		! Saved return value of _get_bus
	.comm	unchar, 2	! Char returned by ungetch(c)
	.comm	line, 2		! Serial line I/O port to copy console I/O to.


