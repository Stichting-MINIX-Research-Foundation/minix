/*	$NetBSD: mbr.S,v 1.23 2010/12/08 21:56:42 jakllsch Exp $	*/

/*
 * Copyright (c) 1999-2004 The NetBSD Foundation, Inc. 
 * All rights reserved.
 *     
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden, based on an earlier work by Wolfgang Solfrank.
 * Major surgery performed by David Laight.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * i386 master boot code
 */

/* Compile options:
 * BOOTSEL	- bootselector code
 * BOOT_EXTENDED - scan extended partition list (LBA reads)
 * COM_PORT	- do serial io to specified port number
 *		  0..3 => bios port, otherwise actual io_addr
 * COM_BAUD	- initialise serial port baud rate
 *
 * TERSE_ERROR	- terse error messages
 * NO_CHS	- all reads are LBA
 * NO_LBA_CHECK	- no check if bios supports LBA reads
 * NO_BANNER    - do not output title line 'banner'
 */

#ifdef BOOT_EXTENDED
#define NO_CHS	1
#define BOOTSEL	1
#endif

#ifdef COM_PORT
#if COM_PORT < 4
/* The first 4 items in the 40:xx segment are the serial port base addresses */
#define COM_PORT_VAL (0x400 + (COM_PORT * 2))
#else
#define COM_PORT_VAL $COM_PORT
#endif

#if !defined(COM_FREQ)
#define COM_FREQ 1843200
#endif
#else
#undef COM_BAUD
#endif

#ifdef BOOTSEL
#define TERSE_ERROR 1
#endif

#include <machine/asm.h>
#include <sys/bootblock.h>

#define BOOTADDR	0x7c00		/* where we get loaded to */

#define TABENTRYSIZE	(MBR_BS_PARTNAMESIZE + 1)
#define NAMETABSIZE	(MBR_PART_COUNT * TABENTRYSIZE)

#ifdef COM_PORT
/* ASCII values for the keys */
#define	KEY_ACTIVE	'\r'
#define	KEY_DISK1	'a'
#define	KEY_PTN1	'1'
#else
/* Scan values for the various keys we use, as returned by the BIOS */
#define	SCAN_ENTER	0x1c
#define	SCAN_F1		0x3b
#define	SCAN_1		0x2

#define	KEY_ACTIVE	SCAN_ENTER
#define	KEY_DISK1	SCAN_F1
#define	KEY_PTN1	SCAN_1
#endif

/*
 * Minimum and maximum drive number that is considered to be valid.
 */
#define MINDRV		0x80
#define MAXDRV		0x8f

#ifdef TERSE_ERROR
/*
 * Error codes. Done this way to save space.
 */
#define ERR_INVPART	'1'		/* Invalid partition table */
#define ERR_READ	'2'		/* Read error */
#define ERR_NOOS	'3'		/* Magic no. check failed for part. */
#define	ERR_KEY		'?'		/* unknown key press */
#define	ERR_NO_LBA	'L'		/* sector above chs limit */

#define	set_err(err)	movb	$err, %al

#else
#define	set_err(err)	mov	$err, %ax
#endif

	.text
	.code16
/*
 * Move ourselves out of the way first.
 * (to the address we are linked at)
 * and zero our bss
 */
ENTRY(start)
	xor	%ax, %ax
	mov	%ax, %ss
	movw	$BOOTADDR, %sp
	mov	%ax, %es
	mov	%ax, %ds
	movw	$mbr, %di
	mov     $BOOTADDR + (mbr - start), %si
	push	%ax			/* zero for %cs of lret */
	push	%di
	movw	$(bss_start - mbr), %cx
	rep
	movsb				/* relocate code */
	mov	$(bss_end - bss_start + 511)/512, %ch
	rep
	stosw				/* zero bss */
	lret				/* Ensures %cs == 0 */

/*
 * Sanity check the drive number passed by the BIOS. Some BIOSs may not
 * do this and pass garbage.
 */
mbr:
	cmpb	$MAXDRV, %dl		/* relies on MINDRV being 0x80 */
	jle	1f
	movb	$MINDRV, %dl		/* garbage in, boot disk 0 */
1:
	push	%dx			/* save drive number */
	push	%dx			/* twice - for err_msg loop */

#if defined(COM_BAUD)
	mov	$com_args, %si
	mov	$num_com_args, %cl	/* %ch is zero from above */
	mov	COM_PORT_VAL, %dx
1:	lodsw
	add	%ah, %dl
	outb	%dx
	loop	1b
#endif

#ifndef NO_BANNER
	mov	$banner, %si
	call	message_crlf
#endif

/*
 * Walk through the selector (name) table printing used entries.
 *
 * Register use:
 * %ax			temp
 * %bx	nametab[]	boot seletor menu
 * %ecx			base of 'extended' partition
 * %edx			next extended partition
 * %si			message ptr (etc)
 * %edi			sector number of this partition
 * %bp	parttab[]	mbr partition table
 */
bootsel_menu:
	movw	$nametab, %bx
#ifdef BOOT_EXTENDED
	xorl	%ecx, %ecx		/* base of extended partition */
next_extended:
	xorl	%edx, %edx		/* for next extended partition */
#endif
	lea	parttab - nametab(%bx), %bp
next_ptn:
	movb	4(%bp), %al		/* partition type */
#ifdef NO_CHS
	movl	8(%bp), %edi		/* partition sector number */
#ifdef BOOT_EXTENDED
	cmpb	$MBR_PTYPE_EXT, %al	/* Extended partition */
	je	1f
	cmpb	$MBR_PTYPE_EXT_LBA, %al	/* Extended LBA partition */
	je	1f
	cmpb	$MBR_PTYPE_EXT_LNX, %al	/* Linux extended partition */
	jne	2f
1:	movl	%edi, %edx		/* save next extended ptn */
	jmp	4f
2:
#endif
	addl	lba_sector, %edi	/* add in extended ptn base */
#endif
	test	%al, %al		/* undefined partition */
	je	4f
	cmpb	$0x80, (%bp)		/* check for active partition */
	jne	3f			/* jump if not... */
#define	ACTIVE	(4 * ((KEY_ACTIVE - KEY_DISK1) & 0xff))
#ifdef NO_CHS
	movl	%edi, ptn_list + ACTIVE	/* save location of active ptn */
#else
	mov	%bp, ptn_list + ACTIVE
#endif
#undef ACTIVE
3:
#ifdef BOOTSEL
	cmpb	$0, (%bx)		/* check for prompt */
	jz	4f
	/* output menu item */
	movw	$prefix, %si
	incb	(%si)
	call	message			/* menu number */
	mov	(%si), %si		/* ':' << 8 | '1' + count */
	shl	$2, %si			/* const + count * 4 */
#define	CONST	(4 * ((':' << 8) + '1' - ((KEY_PTN1 - KEY_DISK1) & 0xff)))
#ifdef NO_CHS
	movl	%edi, ptn_list - CONST(%si)	/* sector to read */
#else
	mov	%bp, ptn_list - CONST(%si)	/* partition info */
#endif
#undef CONST
	mov	%bx, %si
	call	message_crlf			/* prompt */
#endif
4:
	add	$0x10, %bp
	add	$TABENTRYSIZE, %bx
	cmpb	$(nametab - start - 0x100) + 4 * TABENTRYSIZE, %bl
	jne	next_ptn

#ifdef BOOT_EXTENDED
/*
 * Now check extended partition chain
 */
	testl	%edx, %edx
	je	wait_key
	testl	%ecx, %ecx
	jne	1f
	xchg	%ecx, %edx		/* save base of ext ptn chain */
1:	addl	%ecx, %edx		/* sector to read */
	movl	%edx, lba_sector
	movw	$lba_info, %si
	movb	$0x42, %ah
	pop	%dx			/* recover drive # */
	push	%dx			/* save drive */
	int	$0x13
	movw	$BOOTADDR + (nametab - start), %bx
	jnc	next_extended		/* abort menu on read fail */
#endif

/*
 * The non-bootsel code traverses this code path, it needs the
 * correct keycode to select the active partition.
 */

#ifndef BOOTSEL
	mov	$(KEY_ACTIVE - KEY_DISK1) & 0xff, %ax
#else
/*
 * Get the initial time value for the timeout comparison. It is returned
 * by int 1a in cx:dx. We do sums modulo 2^16 so it doesn't matter if
 * the counter wraps (which it does every hour) - so we can safely
 * ignore 'cx'.
 *
 * Loop around checking for a keypress until we have one, or timeout is
 * reached.
 */
wait_key:
	xorb	%ah, %ah
	int	$0x1a
	mov	%dx, %di		/* start time to di */
3:
#ifdef COM_PORT_VAL
	mov	COM_PORT_VAL, %dx
	push	%dx
	add	$5, %dx
	inb	%dx
	pop	%dx
	test	$1, %al
	jz	1f
	inb	%dx
	jmp	check_key
#else
	movb	$1, %ah			/* looks to see if a */
	int	$0x16			/* key has been pressed */
	jz	1f
get_key:
	xorb	%ah, %ah
	int	$0x16			/* 'read key', code ah, ascii al */
	shr	$8, %ax			/* code in %al, %ah zero */
	jmp	check_key
#endif

1:	xorb	%ah, %ah
	int	$0x1a			/* current time to cx:dx */
	sub	%di, %dx
	cmpw	timeout, %dx		/* always wait for 1 tick... */
	jbe	3b			/* 0xffff means never timeout */
def_key:
	mov	defkey, %al		/* timedout - we need %ah to still be zero! */

/*
 * We have a keycode, see what it means.
 * If we don't know we generate error '?' and go ask again
 */
check_key:
/*
 * F1-F10 -> boot disk 0-9. Check if the requested disk isn't above
 * the number of disks actually in the system as stored in 0:0475 by
 * the BIOS.
 * If we trust loc 475, we needn't check the upper bound on the keystroke
 * This is always sector 0, so always read using chs.
 */
	subb	$KEY_DISK1, %al
	cmpb	0x0475, %al
	jae	boot_ptn
	addb	$0x80, %al
	pop	%dx			/* dump saved drive # */
	push	%ax			/* replace with new */
#ifdef NO_CHS
	xorl	%ebp, %ebp		/* read sector number 0 */
	jmp	boot_lba
#else
	movw	$chs_zero, %si		/* chs read sector zero info */
	jmp	read_chs
#endif
#endif	/* BOOTSEL */

/*
 * Boot requested partition.
 * Use keycode to index the table we generated when we scanned the mbr
 * while generating the menu.
 *
 * We very carfully saved the values in the correct part of the table.
 */

boot_ptn:
	shl	$2, %ax
	movw	%ax, %si
#ifdef NO_CHS
	movl	ptn_list(%si), %ebp
	testl	%ebp, %ebp
	jnz	boot_lba
#else
	mov	ptn_list(%si), %si
	test	%si, %si
	jnz	boot_si
#endif
#ifdef BOOTSEL
	set_err(ERR_KEY)
#else
	set_err(ERR_INVPART)
#endif
  /*	jmp	err_msg */

/* Something went wrong...
 * Output error code,
 * reset disk subsystem - needed after read failure,
 * and wait for user key
 */
err_msg:
#ifdef TERSE_ERROR
	movb	%al, errcod
	movw	$errtxt, %si
	call	message
#else
	push	%ax
	movw	$errtxt, %si
	call	message
	pop	%si
	call	message_crlf
#endif
	pop	%dx			/* drive we errored on */
	xor	%ax,%ax			/* only need %ah = 0 */
	int	$0x13			/* reset disk subsystem */
#ifdef BOOTSEL
	pop	%dx			/* original drive number */
	push	%dx
	push	%dx
#ifdef COM_PORT_VAL
	jmp	wait_key		/* Read with timeout (again) */
#else
	jmp	get_key			/* Blocking read */
#endif
#else
	int	$0x18			/* BIOS might ask for a key */
					/* press and retry boot seq. */
1:	sti
	hlt
	jmp	1b
#endif

#ifndef NO_CHS
/*
 * Active partition pointed to by si.
 * Read the first sector.
 *
 * We can either do a CHS (Cylinder Head Sector) or an LBA (Logical
 * Block Address) read.  Always doing the LBA one
 * would be nice - unfortunately not all systems support it.
 * Also some may contain a separate (eg SCSI) bios that doesn't
 * support it even when the main bios does.
 *
 * There is also the additional problem that the CHS values may be wrong
 * (eg if fdisk was run on a different system that used different BIOS
 * geometry).  We convert the CHS value to a LBA sector number using
 * the geometry from the BIOS, if the number matches we do a CHS read.
 */
boot_si:
	movl	8(%si), %ebp		/* get sector # */

	testb	$MBR_BS_READ_LBA, flags
	jnz	boot_lba		/* fdisk forced LBA read */

	pop	%dx			/* collect saved drive... */
	push	%dx			/* ...number to dl */
	movb	$8, %ah
	int	$0x13			/* chs info */

/*
 * Validate geometry, if the CHS sector number doesn't match the LBA one
 * we'll do an LBA read.
 * calc: (cylinder * number_of_heads + head) * number_of_sectors + sector
 * and compare against LBA sector number.
 * Take a slight 'flier' and assume we can just check 16bits (very likely
 * to be true because the number of sectors per track is 63).
 */
	movw	2(%si), %ax		/* cylinder + sector */
	push	%ax			/* save for sector */
	shr	$6, %al
	xchgb	%al, %ah		/* 10 bit cylinder number */
	shr	$8, %dx			/* last head */
	inc	%dx			/* number of heads */
	mul	%dx
	mov	1(%si), %dl		/* head we want */
	add	%dx, %ax
	and	$0x3f, %cx		/* number of sectors */
	mul	%cx
	pop	%dx			/* recover sector we want */
	and	$0x3f, %dx
	add	%dx, %ax
	dec	%ax

	cmp	%bp, %ax
	je	read_chs

#ifndef NO_LBA_CHECK
/*
 * Determine whether we have int13-extensions, by calling int 13, function 41.
 * Check for the magic number returned, and the disk packet capability.
 */
	movw	$0x55aa, %bx
	movb	$0x41, %ah
	pop	%dx
	push	%dx
	int	$0x13
	set_err(ERR_NO_LBA)
	jc	err_msg			/* no int13 extensions */
	cmpw	$0xaa55, %bx
	jnz	err_msg
	testb	$1, %cl
	jz	err_msg
#endif	/* NO_LBA_CHECK */
#endif	/* NO_CHS */

/*
 * Save sector number (passed in %ebp) into lba parameter block,
 * read the sector and leap into it.
 */
boot_lba:
	movl	%ebp, lba_sector	/* save sector number */
	movw	$lba_info, %si
	movb	$0x42, %ah
	pop	%dx			/* recover drive # */
do_read:
	push	%dx			/* save drive */
	int	$0x13

	set_err(ERR_READ)
	jc	err_msg

/*
 * Check signature for valid bootcode
 */
	movb	BOOTADDR, %al		/* first byte non-zero */
	test	%al, %al
	jz	1f
	movw	BOOTADDR + MBR_MAGIC_OFFSET, %ax
1:	cmp	$MBR_MAGIC, %ax
	set_err(ERR_NOOS)
	jnz	err_msg

/* We pass the sector number through to the next stage boot.
 * It doesn't have to use it (indeed no other mbr code will generate) it,
 * but it does let us have a NetBSD pbr that can identify where it was
 * read from!  This lets us use this code to select between two
 * NetBSD system on the same physical driver.
 * (If we've read the mbr of a different disk, it gets a random number
 * - but it wasn't expecting anything...)
*/
	movl	%ebp, %esi
	pop	%dx			/* recover drive # */
#if defined(__minix)
	/* There some bug in MINIX tools, which causes to
	 * generate wrong code for "jmp BOOTADDR" (relocation issue).
	 */
	jmp	start - LOADADDR + BOOTADDR
#else
	jmp	BOOTADDR
#endif /* defined(__minix) */


#ifndef NO_CHS
/*
 * Sector below CHS limit
 * Do a cylinder-head-sector read instead.
 */
read_chs:
	pop	%dx			/* recover drive # */
	movb	1(%si), %dh		/* head */
	movw	2(%si), %cx		/* ch=cyl, cl=sect */
	movw	$BOOTADDR, %bx		/* es:bx is buffer */
	movw	$0x201, %ax		/* command 2, 1 sector */
	jmp	do_read
#endif

/*
 * Control block for int-13 LBA read.
 * We need a xx, 00, 01, 00 somewhere to load chs for sector zero,
 * by a complete fluke there is one here!
 */
chs_zero:
lba_info:
	.word	0x10			/* control block length */
	.word	1			/* sector count */
	.word	BOOTADDR		/* offset in segment */
	.word	0			/* segment */
lba_sector:
	.long	0x0000			/* sector # goes here... */
	.long	0x0000

errtxt: .ascii	"Error "		/* runs into crlf if errcod set */
errcod: .byte	0
crlf:	.asciz	"\r\n"

#ifndef NO_BANNER
#ifdef BOOTSEL
#ifdef COM_PORT_VAL
banner:	.asciz	"a: disk"
#else
banner:	.asciz	"Fn: diskn"
#endif
#else
banner:	.asciz	"NetBSD MBR boot"
#endif
#endif

#ifdef BOOTSEL
prefix:	.asciz	"0: "
#endif

#ifndef TERSE_ERROR
ERR_INVPART:	.asciz	"No active partition"
ERR_READ:	.asciz	"Disk read error"
ERR_NOOS:	.asciz	"No operating system"
#ifndef NO_LBA_CHECK
ERR_NO_LBA:	.asciz	"Invalid CHS read"
#endif
#ifdef BOOTSEL
ERR_KEY:	.asciz	"bad key"
#endif
#endif

#if defined(COM_BAUD)
#define COM_DIVISOR (((COM_FREQ / COM_BAUD) + 8) / 16)
com_args:
	.byte	0x80			/* divisor latch enable */
	.byte	+3			/* io_port + 3 */
	.byte	COM_DIVISOR & 0xff
	.byte	-3			/* io_port */
	.byte	COM_DIVISOR >> 8	/* high baud */
	.byte	+1			/* io_port + 1 */
	.byte	0x03			/* 8 bit no parity */
	.byte	+2			/* io_port + 3 */
num_com_args = (. - com_args)/2
#endif

/*
 * I hate #including source files, but the stuff below has to be at
 * the correct absolute address.
 * Clearly this could be done with a linker script.
 */

message_crlf:
	call	message
	movw	$crlf, %si
#include <message.S>
#if 0
#include <dump_eax.S>
#endif

/*
 * Stuff from here on is overwritten by fdisk - the offset must not change...
 *
 * Get amount of space to makefile can report it.
 * (Unfortunately I can't seem to get the value reported when it is -ve)
 */
mbr_space	= defkey - .
	. = start + MBR_BS_OFFSET
/*
 * Default action, as a keyvalue we'd normally read from the BIOS.
 */
defkey:
	.byte	KEY_ACTIVE		/* ps/2 code */
#ifndef BOOTSEL_FLAGS
#define	BOOTSEL_FLAGS	0
#endif
flags:	.byte	MBR_BS_NEWMBR | BOOTSEL_FLAGS
/*
 * Timeout value. ~65536 ticks per hour, which is ~18.2 times per second.
 * 0xffff means never timeout.
 */
timeout:
	.word	182			/* default to 10 seconds */
/*
 * mbr_bootsel
 */
nametab:
	.fill	MBR_PART_COUNT * (MBR_BS_PARTNAMESIZE + 1), 0x01, 0x00

/* space for mbr_dsn */
	. = start + MBR_DSN_OFFSET
	.long	0

/* mbr_bootsel_magic */
	. = start + MBR_BS_MAGIC_OFFSET
	.word	MBR_BS_MAGIC

/*
 * MBR partition table
 */
	. = start + MBR_PART_OFFSET
parttab:
	.fill	0x40, 0x01, 0x00

	. = start + MBR_MAGIC_OFFSET
	.word	MBR_MAGIC

/* zeroed data space */
bss_off = 0
bss_start = .
#define BSS(name, size) name = bss_start + bss_off; bss_off = bss_off + size
	BSS(ptn_list, 256 * 4)		/* long[]: boot sector numbers */
	BSS(dump_eax_buff, 16)
	BSS(bss_end, 0)
