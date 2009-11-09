!	Mkfhead.s - DOS & BIOS support for mkfile.c	Author: Kees J. Bot
!								9 May 1998
!
! This file contains the startup and low level support for the MKFILE.COM
! utility.  See doshead.ack.s for more comments on .COM files.
!
.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .text

.define _PSP
_PSP:
	.space	256			! Program Segment Prefix

mkfile:
	cld				! C compiler wants UP
	xor	ax, ax			! Zero
	mov	di, _edata		! Start of bss is at end of data
	mov	cx, _end		! End of bss (begin of heap)
	sub	cx, di			! Number of bss bytes
	shr	cx, 1			! Number of words
 rep	stos				! Clear bss

	xor	cx, cx			! cx = argc
	xor	bx, bx
	push	bx			! argv[argc] = NULL
	movb	bl, (_PSP+0x80)		! Argument byte count
0:	movb	_PSP+0x81(bx), ch	! Null terminate
	dec	bx
	js	9f
	cmpb	_PSP+0x81(bx), 0x20	! Whitespace?
	jbe	0b
1:	dec	bx			! One argument character
	js	2f
	cmpb	_PSP+0x81(bx), 0x20	! More argument characters?
	ja	1b
2:	lea	ax, _PSP+0x81+1(bx)	! Address of argument
	push	ax			! argv[n]
	inc	cx			! argc++;
	test	bx, bx
	jns	0b			! More arguments?
9:	movb	_PSP+0x81(bx), ch	! Make a null string
	lea	ax, _PSP+0x81(bx)
	push	ax			! to use as argv[0]
	inc	cx			! Final value of argc
	mov	ax, sp
	push	ax			! argv
	push	cx			! argc
	call	_main			! main(argc, argv)
	push	ax
	call	_exit			! exit(main(argc, argv))

! int creat(const char *path, mode_t mode)
!	Create a file with the old creat() call.
.define _creat
_creat:
	mov	bx, sp
	mov	dx, 2(bx)		! Filename
	xor	cx, cx			! Ignore mode, always read-write
	movb	ah, 0x3C		! "CREAT"
dos:	int	0x21			! ax = creat(path, 0666);
	jc	seterrno
	ret

seterrno:
	mov	(_errno), ax		! Set errno to the DOS error code
	mov	ax, -1
	cwd				! return -1L;
	ret

! int open(const char *path, int oflag)
!	Open a file with the oldfashioned two-argument open() call.
.define _open
_open:
	mov	bx, sp
	mov	dx, 2(bx)		! Filename
	movb	al, 4(bx)		! O_RDONLY, O_WRONLY, O_RDWR
	movb	ah, 0x3D		! "OPEN"
	jmp	dos

! int close(int fd)
!	Close an open file.
.define _close
_close:
	mov	bx, sp
	mov	bx, 2(bx)		! bx = file handle
	movb	ah, 0x3E		! "CLOSE"
	jmp	dos

! void exit(int status)
! void _exit(int status)
!	Return to DOS.
.define _exit, __exit, ___exit
_exit:
__exit:
___exit:
	pop	ax
	pop	ax			! al = status
	movb	ah, 0x4C		! "EXIT"
	int	0x21
	hlt

! ssize_t read(int fd, void *buf, size_t n)
!	Read bytes from an open file.
.define _read
_read:
	mov	bx, sp
	mov	cx, 6(bx)
	mov	dx, 4(bx)
	mov	bx, 2(bx)
	movb	ah, 0x3F		! "READ"
	jmp	dos

! ssize_t write(int fd, const void *buf, size_t n)
!	Write bytes to an open file.
.define _write
_write:
	mov	bx, sp
	mov	cx, 6(bx)
	mov	dx, 4(bx)
	mov	bx, 2(bx)
	movb	ah, 0x40		! "WRITE"
	jmp	dos

! off_t lseek(int fd, off_t offset, int whence)
!	Set file position for read or write.
.define _lseek
_lseek:
	mov	bx, sp
	movb	al, 8(bx)		! SEEK_SET, SEEK_CUR, SEEK_END
	mov	dx, 4(bx)
	mov	cx, 6(bx)		! cx:dx = offset
	mov	bx, 2(bx)
	movb	ah, 0x42		! "LSEEK"
	jmp	dos

!
! $PchId: mkfhead.ack.s,v 1.3 1999/01/14 21:17:06 philip Exp $
