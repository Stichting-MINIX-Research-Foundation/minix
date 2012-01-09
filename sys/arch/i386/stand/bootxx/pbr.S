/*	$NetBSD: pbr.S,v 1.20 2011/08/17 00:07:38 jakllsch Exp $	*/

/*-
 * Copyright (c) 2003,2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Laight.
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
 * i386 partition boot code
 *
 * This code resides in sector zero of the netbsd partition, or sector
 * zero of an unpartitioned disk (eg a floppy).
 * Sector 1 is assumed to contain the netbsd disklabel.
 * Sectors 2 until the end of the track contain the next phase of bootstrap.
 * Which know how to read the interactive 'boot' program from filestore.
 * The job of this code is to read in the phase 1 bootstrap.
 *
 * Makefile supplies:
 * PRIMARY_LOAD_ADDRESS:	Address we load code to (0x1000).
 * BOOTXX_SECTORS:		Number of sectors we load (15).
 * X86_BOOT_MAGIC_1:		A random magic number.
 *
 * Although this code is executing at 0x7c00, it is linked to address 0x1000.
 * All data references MUST be fixed up using R().
 */

#include <machine/asm.h>
#include <sys/bootblock.h>

#define	OURADDR		0x7c00		/* our address */
#define BOOTADDR	PRIMARY_LOAD_ADDRESS

#define R(a) (a - BOOTADDR + OURADDR)

#define lba_info R(_lba_info)
#define lba_sector R(_lba_sector)
#define errtxt R(_errtxt)
#define errcod R(_errcod)
#define newline R(_newline)

#define TABENTRYSIZE	(MBR_BS_PARTNAMESIZE + 1)
#define NAMETABSIZE	(4 * TABENTRYSIZE)

#ifdef BOOT_FROM_FAT
#define MBR_AFTERBPB	90		/* BPB size in FAT32 partition BR */
#else
#define MBR_AFTERBPB	62		/* BPB size in floppy master BR */
#endif

#ifdef TERSE_ERROR
/*
 * Error codes. Done this way to save space.
 */
#define ERR_READ	'2'		/* Read error */
#define ERR_NO_BOOTXX	'B'		/* No bootxx_xfs in 3rd sector */
#define	ERR_PTN		'P'		/* partition not defined */
#define	ERR_NO_LBA	'L'		/* sector above chs limit */

#define	set_err(err)	movb	$err, %al

#else
#define	set_err(err)	mov	$R(err), %ax
#endif

/*
 * This code is loaded to addresss 0:7c00 by either the system BIOS
 * (for a floppy) or the mbr boot code.  Since the boot program will
 * be loaded to address 1000:0, we don't need to relocate ourselves
 * and can load the subsequent blocks (that load boot) to an address
 * of our choosing. 0:1000 is a not unreasonable choice.
 *
 * On entry the BIOS drive number is in %dl and %esi may contain the
 * sector we were loaded from (if we were loaded by NetBSD mbr code).
 * In any case we have to re-read sector zero of the disk and hunt
 * through the BIOS partition table for the NetBSD partition.
 *
 * Or, we may have been loaded by a GPT hybrid MBR, handoff state is
 * specified in T13 EDD-4 annex A.
 */

	.text
	.code16
ENTRY(start)
	/*
	 * The PC BIOS architecture defines a Boot Parameter Block (BPB) here.
	 * The actual format varies between different MS-DOS versions, but
	 * apparently some system BIOS insist on patching this area
	 * (especially on LS120 drives - which I thought had an MBR...).
	 * The initial jmp and nop are part of the standard and may be
	 * tested for by the system BIOS.
	 */
	jmp	start0
	nop
	.ascii	"NetBSD60"		/* oemname (8 bytes) */

	. = start + MBR_BPB_OFFSET	/* move to start of BPB */
					/* (ensures oemname doesn't overflow) */

	. = start + MBR_AFTERBPB	/* skip BPB */
start0:
	xor	%cx, %cx		/* don't trust values of ds, es or ss */
	mov	%cx, %ss
	mov	%cx, %sp
	mov	%cx, %es
#ifndef BOOT_FROM_FAT
	cmpl	$0x54504721, %eax	/* did a GPT hybrid MBR start us? */
	je	boot_gpt
#endif
	mov	%cx, %ds
	xor	%ax, %ax

	/* A 'reset disk system' request is traditional here... */
	push	%dx			/* some BIOS zap %dl here :-( */
	int	$0x13			/* ah == 0 from code above */
	pop	%dx

	/* Read from start of disk */
	incw	%cx			/* track zero sector 1 */
	movb	%ch, %dh		/* dh = head = 0 */
	call	chs_read

/* See if this is our code, if so we have already loaded the next stage */

	xorl	%ebp, %ebp		/* pass sector 0 to next stage */
	movl	(%bx), %eax		/* MBR code shouldn't even have ... */
	cmpl	R(start), %eax		/* ... a jmp at the start. */
	je	pbr_read_ok1

/* Now scan the MBR partition table for a netbsd partition */
	
	xorl	%ebx, %ebx		/* for base extended ptn chain */
scan_ptn_tbl:
	xorl	%ecx, %ecx		/* for next extended ptn */
	movw	$BOOTADDR + MBR_PART_OFFSET, %di
1:	movb	4(%di), %al		/* mbrp_type */
	movl	8(%di), %ebp		/* mbrp_start == LBA sector */
	addl	lba_sector, %ebp	/* add base of extended partition */
#ifdef BOOT_FROM_FAT
	cmpb	$MBR_PTYPE_FAT12, %al
	je	5f
	cmpb	$MBR_PTYPE_FAT16S, %al
	je	5f
	cmpb	$MBR_PTYPE_FAT16B, %al
	je	5f
	cmpb	$MBR_PTYPE_FAT32, %al
	je	5f
	cmpb	$MBR_PTYPE_FAT32L, %al
	je	5f
	cmpb	$MBR_PTYPE_FAT16L, %al
	je	5f
#elif BOOT_FROM_MINIXFS3
	cmpb    $MBR_PTYPE_MINIX_14B, %al
	je      5f
#else
	cmpb	$MBR_PTYPE_NETBSD, %al
#endif
	jne	10f
5:	testl	%esi, %esi		/* looking for a specific sector? */
	je	boot
	cmpl	%ebp, %esi		/* ptn we wanted? */
	je	boot
	/* check for extended partition */
10:	cmpb	$MBR_PTYPE_EXT, %al
	je	15f
	cmpb	$MBR_PTYPE_EXT_LBA, %al
	je	15f
	cmpb	$MBR_PTYPE_EXT_LNX, %al
	jne	20f
15:	movl	8(%di), %ecx		/* sector of next ext. ptn */
20:	add	$0x10, %di
	cmp	$BOOTADDR + MBR_MAGIC_OFFSET, %di
	jne	1b

	/* not in base partitions, check extended ones */
	jecxz	no_netbsd_ptn
	testl	%ebx, %ebx
	jne	30f
	xchgl	%ebx, %ecx		/* save base of ext ptn chain */
30:	addl	%ebx, %ecx		/* address this ptn */
	movl	%ecx, lba_sector	/* sector to read */
	call	read_lba
	jmp	scan_ptn_tbl

no_netbsd_ptn:
	/* Specific sector not found: try again looking for first NetBSD ptn */
	testl	%esi, %esi
	set_err(ERR_PTN)
	jz	error
	xorl	%esi, %esi
	movl	%esi, lba_sector
	jmp	start

/*
 * Sector below CHS limit
 * Do a cylinder-head-sector read instead
 * I believe the BIOS should do reads that cross track boundaries.
 * (but the read should start at the beginning of a track...)
 */
read_chs:
	movb	1(%di), %dh			/* head */
	movw	2(%di), %cx			/* ch=cyl, cl=sect */
	call	chs_read
pbr_read_ok1:
	jmp	pbr_read_ok

/*
 * Active partition pointed to by di.
 *
 * We can either do a CHS (Cylinder Head Sector) or an LBA (Logical
 * Block Address) read.  Always doing the LBA one
 * would be nice - unfortunately not all systems support it.
 * Also some may contain a separate (eg SCSI) BIOS that doesn't
 * support it even when the main BIOS does.
 *
 * The safest thing seems to be to find out whether the sector we
 * want is inside the CHS sector count.  If it is we use CHS, if
 * outside we use LBA.
 *
 * Actually we check that the CHS values reference the LBA sector,
 * if not we assume that the LBA sector is above the limit, or that
 * the geometry used (by fdisk) isn't correct.
 */
boot:
	movl	%ebp, lba_sector	/* to control block */
	testl	%ebx, %ebx		/* was it an extended ptn? */
	jnz	boot_lba		/* yes - boot with LBA reads */

/* get CHS values from BIOS */
	push	%dx				/* save drive number */
	movb	$8, %ah
	int	$0x13				/* chs info */

/*
 * Validate geometry, if the CHS sector number doesn't match the LBA one
 * we'll do an LBA read.
 * calc: (cylinder * number_of_heads + head) * number_of_sectors + sector
 * and compare against LBA sector number.
 * Take a slight 'flier' and assume we can just check 16bits (very likely
 * to be true because the number of sectors per track is 63).
 */
	movw	2(%di), %ax			/* cylinder + sector */
	push	%ax				/* save for sector */
	shr	$6, %al
	xchgb	%al, %ah			/* 10 bit cylinder number */
	shr	$8, %dx				/* last head */
	inc	%dx				/* number of heads */
	mul	%dx
	mov	1(%di), %dl			/* head we want */
	add	%dx, %ax
	and	$0x3f, %cx			/* number of sectors */
	mul	%cx
	pop	%dx				/* recover sector we want */
	and	$0x3f, %dx
	add	%dx, %ax
	dec	%ax
	pop	%dx				/* recover drive nmber */

	cmp	%bp, %ax
	je	read_chs

check_lba:
#ifdef NO_LBA_CHECK
	jmp	boot_lba
#else
/*
 * Determine whether we have int13-extensions, by calling
 * int 13, function 41. Check for the magic number returned,
 * and the disk packet capability.
 *
 * This is actually relatively pointless:
 * 1) we only use LBA reads if CHS ones would fail
 * 2) the MBR code managed to read the same sectors
 * 3) the BIOS will (ok should) reject the LBA read as a bad BIOS call
 */
	movw	$0x55aa, %bx
	movb	$0x41, %ah
	int	$0x13
	jc	1f				/* no int13 extensions */
	cmpw	$0xaa55, %bx
	jnz	1f
	testb	$1, %cl
	jnz	boot_lba
1:	set_err(ERR_NO_LBA)
#endif	/* NO_LBA_CHECK */

/*
 * Something went wrong,
 * Output error code,
 */

error:
#ifdef TERSE_ERROR
	movb	%al, errcod
	movw	$errtxt, %si
	call	message
#else
	push	%ax
	movw	$errtxt, %si
	call	message
	pop	%si
	call	message
	movw	$newline, %si
	call	message
#endif
1:	sti
	hlt
	jmp	1b

boot_lba:
	call	read_lba

/*
 * Check magic number for valid stage 2 bootcode
 * then jump into it.
 */
pbr_read_ok:
	cmpl	$X86_BOOT_MAGIC_1, bootxx_magic
	set_err(ERR_NO_BOOTXX)
	jnz	error

	movl	%ebp, %esi			/* %esi ptn base, %dl disk id */
	movl	lba_sector + 4, %edi		/* %edi ptn base high */
	jmp	$0, $bootxx			/* our %cs may not be zero */

/* Read disk using int13-extension parameter block */
read_lba:
	pusha
	movw	$lba_info, %si			/* ds:si is ctl block */
	movb	$0x42, %ah
do_read:
	int	$0x13
	popa

	set_err(ERR_READ)
	jc	error
	ret

/* Read using CHS */

chs_read:
	movw	$BOOTADDR, %bx			/* es:bx is buffer */
	pusha
	movw	$0x200 + BOOTXX_SECTORS, %ax	/* command 2, xx sectors */
	jmp	do_read

#ifndef BOOT_FROM_FAT
boot_gpt:
	movl	(20+32+0)(%si), %ebp
	movl	(20+32+4)(%si), %edi
	movw	%cx, %ds
	movl	%ebp, lba_sector + 0
	movl	%edi, lba_sector + 4
	movl	%ebp, %esi
	jmp	boot_lba
#endif

_errtxt: .ascii	"Error "			/* runs into newline... */
_errcod: .byte	0				/* ... if errcod set */
_newline:
	.asciz	"\r\n"

#ifndef TERSE_ERROR
ERR_READ:	.asciz	"read"
ERR_NO_BOOTXX:	.asciz	"no magic"
ERR_PTN:	.asciz	"no slice"
#ifndef NO_LBA_CHECK
ERR_NO_LBA:	.asciz	"need LBA"
#endif
#endif

/*
 * I hate #including source files, but pbr_magic below has to be at
 * the correct absolute address.
 * Clearly this could be done with a linker script.
 */

#include <message.S>
#if 0
#include <dump_eax.S>
#endif

/* Control block for int-13 LBA read. */
_lba_info:
	.word	0x10				/* control block length */
	.word	BOOTXX_SECTORS			/* sector count */
	.word	BOOTADDR			/* offset in segment */
	.word	0				/* segment */
_lba_sector:
	.quad	0				/* sector # goes here... */

/* Drive Serial Number */
	. = _C_LABEL(start) + MBR_DSN_OFFSET
	.long	0

/* mbr_bootsel_magic (not used here) */
	. = _C_LABEL(start) + MBR_BS_MAGIC_OFFSET
	.word	0

/*
 * Provide empty MBR partition table.
 * If this is installed as an MBR, the user can use fdisk(8) to create
 * the correct partition table ...
 */
	. = _C_LABEL(start) + MBR_PART_OFFSET
_pbr_part0:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.long	0, 0
_pbr_part1:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.long	0, 0
_pbr_part2:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.long	0, 0
_pbr_part3:
	.byte	0, 0, 0, 0, 0, 0, 0, 0	
	.long	0, 0

/*
 * The magic comes last
 */
	. = _C_LABEL(start) + MBR_MAGIC_OFFSET
pbr_magic:
	.word	MBR_MAGIC
