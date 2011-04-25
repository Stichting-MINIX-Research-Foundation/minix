!	jumpboot 1.0 - Jump to another bootstrap	Author: Kees J. Bot
!								14 Apr 1999
!
! This code may be placed into any free boot sector, like the first sector
! of an extended partition, a file system partition other than the root,
! or even the master bootstrap.  It will load and run another bootstrap whose
! disk, partition, and slice number (not necessarily all three) are patched
! into this code by installboot.  If the ALT key is held down when this code
! is booted then you can type the disk, partition, and slice numbers manually.
! The manual interface is default if no numbers are patched in by installboot.
!

	o32	   =	  0x66	! This assembler doesn't know 386 extensions
	LOADOFF	   =	0x7C00	! 0x0000:LOADOFF is where this code is loaded
	BUFFER	   =	0x0600	! First free memory
	PART_TABLE =	   446	! Location of partition table within master
	PENTRYSIZE =	    16	! Size of one partition table entry
	MAGIC	   =	   510	! Location of the AA55 magic number

	! <ibm/partition.h>:
	MINIX_PART =	  0x81
	sysind	   =	     4
	lowsec	   =	     8


.text

! Find and load another bootstrap and jump to it.
jumpboot:
	xor	ax, ax
	mov	ds, ax
	mov	es, ax
	cli
	mov	ss, ax			! ds = es = ss = Vector segment
	mov	sp, #LOADOFF
	sti

! Move this code to safety, then jump to it.
	mov	si, sp			! si = start of this code
	mov	di, #BUFFER		! di = Buffer area
	mov	cx, #512/2		! One sector
	cld
  rep	movs
	jmpf	BUFFER+migrate, 0	! To safety
migrate:

	mov	bp, #BUFFER+guide	! Patched guiding characters
altkey:
	movb	ah, #0x02		! Keyboard shift status
	int	0x16
	testb	al, #0x08		! Bit 3 = ALT key
	jz	noalt			! ALT key pressed?
again:
	mov	bp, #zero		! Ignore patched stuff
noalt:

! Follow guide characters to find the boot partition.
	call	print
	.ascii	"d?\b\0"		! Initial greeting

! Disk number?
disk:
	movb	dl, #0x80 - 0x30	! Prepare to add an ASCII digit
	call	getch			! Get number to tell which disk
	addb	dl, al			! dl = 0x80 + (al - '0')
	jns	n0nboot			! Result should be >= 0x80
	mov	si, #BUFFER+zero-lowsec	! si = where lowsec(si) is zero
	cmpb	(bp), #0x23		! Next guide character is '#'?
	jne	notlogical
	lea	si, 1-lowsec(bp)	! Logical sector offset follows '#'
notlogical:
	call	load			! Load chosen sector of chosen disk
	cmpb	(bp), #0x23
	je	boot			! Run bootstrap if a logical is chosen

	call	print			! Intro to partition number
	.ascii	"p?\b\0"

part:
	call	getch			! Get character to tell partition
	call	gettable		! Get partition table
	call	sort			! Sort partition table
	call	choose_load		! Compute chosen entry and load

	cmpb	sysind(si), #MINIX_PART	! Minix subpartition table possible?
	jne	waitboot

	call	print			! Intro to slice number
	.ascii	"s?\b\0"

slice:
	call	getch			! Get character to tell slice
	call	gettable		! Get partition table
	call	choose_load		! Compute chosen entry and load

waitboot:
	call	print			! Intro to nothing
	.ascii	" ?\b\0"
	call	getch			! Supposed to type RETURN now
n0nboot:jmp	nonboot			! Sorry, can't go further

! Get a character, either the patched-in, or one from the keyboard.
getch:
	movb	al, (bp)	! Get patched-in character
	testb	al, al
	jz	getkey
	inc	bp
	jmp	gotkey
getkey:	xorb	ah, ah		! Wait for keypress
	int	0x16
gotkey:	testb	dl, dl		! Ignore CR if disk number not yet set
	jns	putch
	cmpb	al, #0x0D	! Carriage return?
	je	boot
	!jmp	putch

! Print a character
putch:	movb	ah, #0x0E	! Print character in teletype mode
	mov	bx, #0x0001	! Page 0, foreground color
	int	0x10
	ret

! Print a message.
print:	mov	cx, si		! Save si
	pop	si		! si = String following 'call print'
prnext:	lodsb			! al = *si++ is char to be printed
	testb	al, al		! Null marks end
	jz	prdone
	call	putch
	jmp	prnext
prdone:	xchg	si, cx		! Restore si
	jmp	(cx)		! Continue after the string

! Return typed (or in patched data) means to run the bootstrap now in core!
boot:
	call	print		! Make line on screen look proper
	.ascii	"\b  \r\n\0"
	jmp	LOADOFF-BUFFER	! Jump to LOADOFF

! Compute address of chosen partition entry from choice al into si, then
! continue to load the boot sector of that partition.
choose_load:
	subb	al, #0x30		! al -= '0'
	cmpb	al, #4			! Only four partitions
	ja	n0nboot
	movb	ah, #PENTRYSIZE
	mulb	ah			! al *= PENTRYSIZE
	add	ax, #BUFFER+PART_TABLE
	mov	si, ax			! si = &part_table[al - '0']
	movb	al, sysind(si)		! System indicator
	testb	al, al			! Unused partition?
	jz	n0nboot
	!jmp	load			! Continue to load boot sector

! Load boot sector of the current partition.
load:
	push	dx		! Save drive code
	push	es		! Next call sets es
	movb	ah, #0x08	! Code for drive parameters
	int	0x13
	pop	es
	andb	cl, #0x3F	! cl = max sector number (1-origin)
	incb	dh		! dh = 1 + max head number (0-origin)
	movb	al, cl		! al = cl = sectors per track
	mulb	dh		! dh = heads, ax = heads * sectors
	mov	bx, ax		! bx = sectors per cylinder = heads * sectors
	mov	ax, lowsec+0(si)
	mov	dx, lowsec+2(si) ! dx:ax = sector within drive
	cmp	dx, #[1024*255*63-255]>>16  ! Near 8G limit?
	jae	bigdisk
	div	bx		! ax = cylinder, dx = sector within cylinder
	xchg	ax, dx		! ax = sector within cylinder, dx = cylinder
	movb	ch, dl		! ch = low 8 bits of cylinder
	divb	cl		! al = head, ah = sector (0-origin)
	xorb	dl, dl		! About to shift bits 8-9 of cylinder into dl
	shr	dx, #1
	shr	dx, #1		! dl[6..7] = high cylinder
	orb	dl, ah		! dl[0..5] = sector (0-origin)
	movb	cl, dl		! cl[0..5] = sector, cl[6..7] = high cyl
	incb	cl		! cl[0..5] = sector (1-origin)
	pop	dx		! Restore drive code in dl
	movb	dh, al		! dh = al = head
	mov	bx, #LOADOFF	! es:bx = where sector is loaded
	mov	ax, #0x0201	! ah = Code for read / al = one sector
	int	0x13
	jmp	rdeval		! Evaluate read result
bigdisk:
	mov	bx, dx		! bx:ax = dx:ax = sector to read
	pop	dx		! Restore drive code in dl
	push	si		! Save si
	mov	si, #BUFFER+ext_rw ! si = extended read/write parameter packet
	mov	8(si), ax	! Starting block number = bx:ax
	mov	10(si), bx
	movb	ah, #0x42	! Extended read
	int	0x13
	pop	si		! Restore si to point to partition entry
	!jmp	rdeval
rdeval:
	jnc	rdok
rderr:
	call	print
	.ascii	"\r\nRead error\r\n\0"
	jmp	again
rdok:
	cmp	LOADOFF+MAGIC, #0xAA55
	je	sigok		! Signature ok?
nonboot:
	call	print
	.ascii	"\r\nNot bootable\r\n\0"
	jmp	again
sigok:
	ret

! Get the partition table into my space.
gettable:
	mov	si, #LOADOFF+PART_TABLE
	mov	di, #BUFFER+PART_TABLE
	mov	cx, #4*PENTRYSIZE/2
  rep	movs
	ret

! Sort the partition table.
sort:
	mov	cx, #4			! Four times is enough to sort
bubble:	mov	si, #BUFFER+PART_TABLE	! First table entry
bubble1:lea	di, PENTRYSIZE(si)	! Next entry
	cmpb	sysind(si), ch		! Partition type, nonzero when in use
	jz	exchg			! Unused entries sort to the end
inuse:	mov	bx, lowsec+0(di)
	sub	bx, lowsec+0(si)	! Compute di->lowsec - si->lowsec
	mov	bx, lowsec+2(di)
	sbb	bx, lowsec+2(si)
	jae	order			! In order if si->lowsec <= di->lowsec
exchg:	movb	bl, (si)
	xchgb	bl, PENTRYSIZE(si)	! Exchange entries byte by byte
	movb	(si), bl
	inc	si
	cmp	si, di
	jb	exchg
order:	mov	si, di
	cmp	si, #BUFFER+PART_TABLE+3*PENTRYSIZE
	jb	bubble1
	loop	bubble
	ret

.data

! Extended read/write commands require a parameter packet.
ext_rw:
	.data1	0x10		! Length of extended r/w packet
	.data1	0		! Reserved
	.data2	1		! Blocks to transfer (just one)
	.data2	LOADOFF		! Buffer address offset
	.data2	0		! Buffer address segment
	.data4	0		! Starting block number low 32 bits (tbfi)
zero:	.data4	0		! Starting block number high 32 bits

	.align	2
guide:
! Guide characters and possibly a logical partition number patched here by
! installboot, up to 6 bytes maximum.
