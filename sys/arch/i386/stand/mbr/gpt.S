/*	$NetBSD: gpt.S,v 1.1 2011/01/06 01:08:49 jakllsch Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to the NetBSD Foundation
 * by Mike M. Volokhov, based on the mbr.S code by Wolfgang Solfrank,
 * Frank van der Linden, and David Laight.
 * Development of this software was supported by the
 * Google Summer of Code program.
 * The GSoC project was mentored by Allen Briggs and Joerg Sonnenberger.
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
 * x86 PC BIOS master boot code - GUID partition table format
 */

/* Compile options:
 * COM_PORT	- do serial io to specified port number
 *		  0..3 => bios port, otherwise actual io_addr
 * COM_BAUD	- initialise serial port baud rate
 *
 * NO_LBA_CHECK	- no check if bios supports LBA reads
 * NO_CRC_CHECK	- disable crc checks for GPT
 * NO_BANNER    - do not output title line 'banner'
 */

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
#endif

#include <machine/asm.h>
#include <sys/bootblock.h>

#define BOOTADDR	0x7c00
#define GPTBUFADDR	0xa000		/* GPT buffer (both header & array) */

/*
 * GPT header structure, offsets
 */
#define GPTHDR_SIG_O	0		/* header signature, 8 b */
#define GPTHDR_REV_O	8		/* GPT revision, 4 b */
#define GPTHDR_SIZE_O	12		/* header size, 4 b */
#define GPTHDR_CRC_O	16		/* header CRC32, 4 b */
#define GPTHDR_RSVD_O	20		/* reserved, 4 b */
#define GPTHDR_MYLBA_O	24		/* this header LBA, 8 b */
#define GPTHDR_ALTLBA_O	32		/* alternate header LBA, 8 b */
#define GPTHDR_SLBA_O	40		/* first usable LBA, 8 b */
#define GPTHDR_ELBA_O	48		/* last usable LBA, 8 b */
#define GPTHDR_GUID_O	56		/* disk GUID, 16 b */
#define GPTHDR_PTNLBA_O	72		/* ptn. entry LBA, 8 b */
#define GPTHDR_PTNN_O	80		/* number of ptns, 4 b */
#define GPTHDR_PTNSZ_O	84		/* partition size, 4 b */
#define GPTHDR_PTNCRC_O	88		/* ptn. array CRC32, 4 b */

/*
 * GPT partition entry structure, offsets
 */
#define GPTPTN_TYPE_O	0		/* ptn. type GUID, 16 b */
#define GPTPTN_GUID_O	16		/* ptn. unique GUID, 16 b */
#define GPTPTN_SLBA_O	32		/* ptn. starting LBA, 8 b */
#define GPTPTN_ELBA_O	40		/* ptn. ending LBA, 8 b */
#define GPTPTN_ATTR_O	48		/* ptn. attributes, 8 b */
#define GPTPTN_NAME_O	56		/* ptn. name, UTF-16, 72 b */

/*
 * Default values of generic disk partitioning
 */
#ifndef SECTOR_SIZE
#define SECTOR_SIZE		512		/* 8192 bytes max */
#endif
#define GPTHDR_BLKNO		1
#define GPTHDR_SIZE		92		/* size of GPT header */
#define GPTHDR_LBASZ		1		/* default LBAs for header */
#define GPTHDR_PTNSZ		128		/* size of a partition entry */
#define GPTHDR_PTNN_MIN		128		/* minimum number of ptns */
#define GPTPTN_SIZE		(GPTHDR_PTNSZ * GPTHDR_PTNN_MIN)
#if GPTPTN_SIZE % SECTOR_SIZE == 0
#define GPTPTN_LBASZ		(GPTPTN_SIZE / SECTOR_SIZE)
#else
#define GPTPTN_LBASZ		(GPTPTN_SIZE / SECTOR_SIZE + 1)
#endif
#define GPT_LBASZ		(GPTHDR_LBASZ + GPTPTN_LBASZ)

/*
 * Minimum and maximum drive number that is considered to be valid.
 */
#define MINDRV		0x80
#define MAXDRV		0x8f

/*
 * Error codes. Done this way to save space.
 */
#define ERR_INVPART	'P'		/* Invalid partition table */
#define ERR_READ	'R'		/* Read error */
#define ERR_NOOS	'S'		/* Magic no. check failed for part. */
#define ERR_NO_LBA	'L'		/* Sector above chs limit */

#define set_err(err)	movb	$err, %al

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
	movsb				/* relocate code (zero %cx on exit) */
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

#if defined(COM_PORT) && defined(COM_BAUD)
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
	call	message
#endif

/*
 * Read and validate GUID partition tables
 *
 * Register use:
 * %ax		temp
 * %bx		temp
 * %cx		counters (%ch was preset to zero via %cx before)
 * %dx		disk pointer (inherited from BIOS or the check above)
 * %bp		GPT header pointer
 */
#ifndef NO_LBA_CHECK
/*
 * Determine whether we have int13-extensions, by calling int 13, function 41.
 * Check for the magic number returned, and the disk packet capability.
 * On entry %dx should contain BIOS disk number.
 */
lba_check:
	movw	$0x55aa, %bx
	movb	$0x41, %ah
	int	$0x13
	set_err(ERR_NO_LBA)
	jc	1f			/* no int13 extensions */
	cmpw	$0xaa55, %bx
	jnz	1f
	testb	$1, %cl
	jnz	gpt
1:	jmp	err_msg
#endif

gpt:
	/*
	 * Read GPT header
	 */
	movw	$GPTBUFADDR, %bp	/* start from primary GPT layout */
read_gpt:
	call	do_lba_read		/* read GPT header */
	jc	try_gpt2

	/*
	 * Verify GPT header
	 */
	movw	$efihdr_sign, %si	/* verify GPT signature... */
	movw	%bp, %di		/* ptn signature is at offset 0 */
	movb	$sizeof_efihdr_sign, %cl /* signature length */
	repe
	cmpsb				/* compare */
	jne	try_gpt2		/* if not equal - try GPT2 */

#ifndef NO_CRC_CHECK
	mov	%bp, %si		/* verify CRC32 of the header... */
	mov	$GPTHDR_SIZE, %di	/* header boundary */
	add	%bp, %di
	xor	%eax, %eax
	xchgl	%eax, GPTHDR_CRC_O(%bp)	/* save and reset header CRC */
	call	crc32
	cmp	%eax, %ebx		/* is CRC correct? */
	jne	try_gpt2
#endif

#if 0	/* XXX: weak check - safely disabled due to space constraints */
	movw	$lba_sector, %si	/* verify GPT location... */
	mov	$GPTHDR_MYLBA_O, %di
	add	%bp, %di
	movb	$8, %cl			/* LBA size */
	repe
	cmpsb				/* compare */
	jne	try_gpt2		/* if not equal - try GPT2 */
#endif

	/*
	 * All header checks passed - now verify GPT partitions array
	 */
#ifndef NO_CRC_CHECK
	movl	GPTHDR_PTNCRC_O(%bp), %eax	/* original array checksum */
	push	%bp			/* save header pointer for try_gpt2 */
#endif

	/*
	 * point %bp to GPT partitions array location
	 */
	cmp	$GPTBUFADDR, %bp
	je	1f
	mov	$GPTBUFADDR, %bp
	jmp	2f
1:	mov	$GPTBUFADDR + GPTPTN_LBASZ * SECTOR_SIZE, %bp
2:

#ifndef NO_CRC_CHECK
	mov	%bp, %si		/* array location for CRC32 check */
	mov	$GPTPTN_SIZE, %di	/* array boundary */
	add	%bp, %di
	call	crc32
	cmp	%eax, %ebx		/* is CRC correct? */
	jne	1f			/* if no - try GPT2 */
	pop	%ax			/* restore stack consistency */
#endif
	jmp	gpt_parse

#ifndef NO_CRC_CHECK
1:	pop	%bp			/* restore after unsucc. array check */
#endif
try_gpt2:
	cmp	$GPTBUFADDR, %bp	/* is this GPT1? */
	set_err(ERR_INVPART)		
	jne	err_msg			/* if no - we just tried GPT2. Stop. */

	mov	%bp, %si		/* use [%bp] as tmp buffer */
	movb	$0x1a, (%si)		/* init buffer size (per v.1.0) */
	movb	$0x48, %ah		/* request extended LBA status */
	int	$0x13			/* ... to get GPT2 location */
	set_err(ERR_NO_LBA)
	jc	err_msg			/* on error - stop */
#define LBA_DKINFO_OFFSET 16		/* interested offset in out buffer */
	addw	$LBA_DKINFO_OFFSET, %si	/* ... contains number of disk LBAs */
#undef LBA_DKINFO_OFFSET
	movw	$lba_sector, %di
	movb	$8, %cl			/* LBA size */
	rep
	movsb				/* do get */
	subl	$GPT_LBASZ, lba_sector	/* calculate location of GPT2 */
	sbbl	$0, lba_sector + 4	/* 64-bit LBA correction */

	movw	$GPTBUFADDR + GPTPTN_LBASZ * SECTOR_SIZE, %bp
					/* the GPT2 header location */
	jmp	read_gpt		/* try once again */

/*
 * GPT header validation done.
 * Now parse GPT partitions and try to boot from appropriate.
 * Register use:
 * %bx		partition counter
 * %bp		partition entry pointer (already initialized on entry)
 * %di		partition entry moving pointer
 * %si		variables pointer
 * %cx		counter
 */

gpt_parse:
	movw	$BOOTADDR, lba_rbuff	/* from now we will read boot code */
	movb	$1, lba_count		/* read PBR only */
	mov	$GPTHDR_PTNN_MIN, %bx	/* number of GUID partitions to parse */
do_gpt_parse:
	movw	$bootptn_guid, %si	/* lookup the boot partition GUID */
	movb	$0x10, %cl		/* sizeof GUID */
	movw	%bp, %di		/* set pointer to partition entry */
	add	%cx, %di		/* partition GUID at offset 16 */
	repe
	cmpsb				/* do compare */
	jne	try_nextptn		/* doesn't seem appropriate ptn */

	/*
	 * Read partition boot record
	 */
	mov	$GPTPTN_SLBA_O, %si	/* point %si to partition LBA */
	add	%bp, %si
	movw	$lba_sector, %di
	movb	$8, %cl			/* LBA size */
	rep
	movsb				/* set read pointer to LBA of PBR */
	call	do_lba_read		/* read PBR */
	jz	try_nextptn

	/*
	 * Check signature for valid bootcode and try to boot
	 */
	movb	BOOTADDR, %al		/* first byte non-zero */
	testb	%al, %al
	jz	1f
	movw	BOOTADDR + MBR_MAGIC_OFFSET, %ax
1:	cmp	$MBR_MAGIC, %ax
	jne	try_nextptn
do_boot:
	pop	%dx			/* ... %dx - drive # */
	movw	$lba_sector, %sp	/* ... %ecx:%ebx - boot partition LBA */
	pop	%ebx
	pop	%ecx
	movl 	crc32_poly, %eax	/* X86_MBR_GPT_MAGIC */
	jmp	BOOTADDR
					/* THE END */

try_nextptn:
	addw	$GPTHDR_PTNSZ, %bp	/* move to next partition */
	dec	%bx			/* ptncounter-- */
	jnz	do_gpt_parse
	set_err(ERR_NOOS)		/* no bootable partitions were found */
	/* jmp	err_msg */		/* stop */

/* Something went wrong...
 * Output error code,
 * reset disk subsystem - needed after read failure,
 * and wait for user key
 */
err_msg:
	movb	%al, errcod
	movw	$errtxt, %si
	call	message
	pop	%dx			/* drive we errored on */
	xor	%ax,%ax			/* only need %ah = 0 */
	int	$0x13			/* reset disk subsystem */
	int	$0x18			/* BIOS might ask for a key */
					/* press and retry boot seq. */
1:	sti
	hlt
	jmp	1b

/*
 * I hate #including source files, but the stuff below has to be at
 * the correct absolute address.
 * Clearly this could be done with a linker script.
 */

#if defined(COM_PORT) && defined(COM_BAUD)
message:
	pusha
message_1:
	lodsb
	test	%al, %al
	jz	3f
	mov	COM_PORT_VAL, %dx
	outb	%al, %dx
	add	$5, %dl
2:	inb	%dx
	test	$0x40, %al
	jz	2b
	jmp	message_1
3:	popa
	ret
#else
#include <message.S>
#endif

#if 0
#include <dump_eax.S>
#endif

#ifndef NO_CRC_CHECK
/*
 * The CRC32 calculation
 *
 * %si		address of block to hash
 * %di		stop address
 * %ax		scratch (but restored after exit)
 * %dx		scratch (but restored after exit)
 * %cx		counter
 * %ebx		crc (returned)
 */
crc32:
	push	%dx			/* preserve drive number */
	push	%ax			/* preserve original CRC */

	xorl	%ebx, %ebx
	decl	%ebx			/* init value */
1:
	lodsb				/* load next message byte to %al */
	movb	$8, %cl			/* set bit counter */
2:
	movb	%al, %dl
	xorb	%bl, %dl		/* xoring with previous result */
	shrl	$1, %ebx
	shrb	$1, %al
	testb	$1, %dl
	jz	3f
crc32_poly = . + 3			/* gross, but saves a few bytes */
	xorl	$0xedb88320, %ebx	/* EFI CRC32 Polynomial */
3:
	loop	2b			/* loop over bits */
	cmp	%di, %si		/* do we reached end of message? */
	jne	1b
	notl	%ebx			/* result correction */

	pop	%ax
	pop	%dx
	ret
#endif

do_lba_read:
	movw	$lba_dap, %si
	movb	$0x42, %ah
	int	$0x13			/* read */
	ret

/*
 * Data definition block
 */

errtxt: .ascii	"Error "		/* runs into crlf if errcod set */
errcod: .byte	0
crlf:	.asciz	"\r\n"

#ifndef NO_BANNER
banner:	.asciz	"NetBSD GPT\r\n"
#endif

#if defined(COM_PORT) && defined(COM_BAUD)
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
 * Control block for int-13 LBA read - Disk Address Packet
 */
lba_dap:
	.byte	0x10			/* control block length */
	.byte	0			/* reserved */
lba_count:
	.word	GPT_LBASZ		/* sector count */
lba_rbuff:
	.word	GPTBUFADDR		/* offset in segment */
	.word	0			/* segment */
lba_sector:
	.quad	GPTHDR_BLKNO		/* sector # goes here... */

efihdr_sign:
	.ascii	"EFI PART"		/* GPT header signature */
	.long	0x00010000		/* GPT header revision */
sizeof_efihdr_sign = . - efihdr_sign

/*
 * Stuff from here on is overwritten by gpt/fdisk - the offset must not change
 *
 * Get amount of space to makefile can report it.
 * (Unfortunately I can't seem to get the value reported when it is -ve)
 */
mbr_space = bootptn_guid - .

/*
 * GUID of the bootable partition. Patchable area.
 * Default GUID used by installer for safety checks.
 */
	. = start + MBR_GPT_GUID_OFFSET
bootptn_guid:
	/* MBR_GPT_GUID_DEFAULT */
	.long 0xeee69d04
	.word 0x02f4
	.word 0x11e0
	.byte 0x8f,0x5d
	.byte 0x00,0xe0,0x81,0x52,0x9a,0x6b

/* space for mbr_dsn */
	. = start + MBR_DSN_OFFSET
	.long	0

/* mbr_bootsel_magic */
	. = start + MBR_BS_MAGIC_OFFSET
	.word	0

/* mbr partition table */
	. = start + MBR_PART_OFFSET
parttab:
	.fill	0x40, 0x01, 0x00

	. = start + MBR_MAGIC_OFFSET
	.word	MBR_MAGIC

/* zeroed data space */
bss_off = 0
bss_start = .
#define BSS(name, size) name = bss_start + bss_off; bss_off = bss_off + size
	BSS(dump_eax_buff, 16)
	BSS(bss_end, 0)
