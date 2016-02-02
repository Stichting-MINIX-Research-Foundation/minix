/*	$NetBSD: cdboot.S,v 1.12 2011/01/04 16:53:05 jakllsch Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bang Jun-Young.
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
 * This is a primary boot loader that loads a secondary boot loader
 * directly from CD without performing floppy/hard disk emulation as
 * described by the El Torito specification.
 */

#include <machine/asm.h>
#include <sys/bootblock.h>

#define BOOT_ADDR	0x7c00
#define BLOCK_SIZE	2048		/* Default for ISO 9660 */
#define VD_LBA		16		/* LBA of Volume Descriptor (VD) */
#define PVD_ADDR	end		/* Where Primary VD is loaded */
#define ROOTDIR_ADDR	end+BLOCK_SIZE	/* Where Root Directory is loaded */
#define LOADER_ADDR	SECONDARY_LOAD_ADDRESS

#ifdef BOOT_FROM_FAT
#define MBR_AFTERBPB	90		/* BPB size in FAT32 partition BR */
#else
#define MBR_AFTERBPB	62		/* BPB size in floppy master BR */
#endif

/*
 * See src/sys/sys/bootblock.h for details.
 */
#define MBR_PART_COUNT	4
#define MBR_PART_OFFSET	446
#define MBR_PART_SIZE	16		/* sizeof(struct mbr_partition) */

/*
 * Disk error codes
 */
#define ERROR_TIMEOUT	0x80

/*
 * Volume Descriptor types.
 */
#define VD_PRIMARY		1
#define VD_SUPPLEMENTARY	2
#define VD_TERMINATOR		255

/* Only actually used entries are listed below */

/*
 * Format of Primary Volume Descriptor (8.4)
 */
#define PVD_ROOT_DR	156	/* Offset of Root Directory Record */

/*
 * Format of Directory Record (9.1)
 */
#define DR_LEN		0
#define DR_EXTENT	2
#define DR_DATA_LEN	10
#define DR_NAME_LEN	32
#define DR_NAME		33

	.text
	.code16
ENTRY(start)
	jmp	start1

	. = start + MBR_AFTERBPB	/* skip BPB */
	. = start + MBR_DSN_OFFSET
	.long	0

/* mbr_bootsel_magic (not used here) */
	. = start + MBR_BS_MAGIC_OFFSET
	.word	0

	. = start + MBR_PART_OFFSET
	. = start + MBR_MAGIC_OFFSET
pbr_magic:
	.word	MBR_MAGIC
	.fill	512			/* reserve space for disklabel */
start1:
	jmp	1f
	.balign	4
	.long	X86_BOOT_MAGIC_1	/* checked by installboot & pbr code */
boot_params:				/* space for patchable variables */
	.long	1f - boot_params	/* length of this data area */
#include <boot_params.S>
	. = start1 + 0x80		/* Space for patching unknown params */

1:	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	$BOOT_ADDR, %sp
	movw	%sp, %si
	movw	$start, %di
	movw	$BLOCK_SIZE/2, %cx
	rep
	movsw
	ljmp	$0, $real_start

real_start:
	movb	%dl, boot_drive		/* Save boot drive number */

#ifndef DISABLE_KEYPRESS
	/*
	 * We can skip boot wait when:
	 *  - there's no hard disk present.
	 *  - there's no active partition in the MBR of the 1st hard disk.
	 */

	/*
	 * Check presence of hard disks.
	 */
	movw	$0x475, %si
	movb	(%si), %al
	testb	%al, %al
	jz	boot_cdrom

	/*
	 * Find the active partition from the MBR.
	 */
	movw	$0x0201, %ax		/* %al = number of sectors to read */
	movw	$BOOT_ADDR, %bx		/* %es:%bx = data buffer */
	movw	$0x0001, %cx		/* %ch = low 8 bits of cylinder no */
					/* %cl = high 2 bits of cyl no & */
					/*       sector number */
	movw	$0x0080, %dx		/* %dh = head number */
					/* %dl = disk number */
	int	$0x13			/* Read MBR into memory */
	jc	boot_cdrom		/* CF set on error */

	movb	$1, mbr_loaded
	movb	$MBR_PART_COUNT, %cl
	movw	$BOOT_ADDR+MBR_PART_OFFSET, %si
1:
	movb	(%si), %al
	testb	$0x80, %al
	jnz	found_active
	addw	$MBR_PART_SIZE, %si
	decb	%cl
	testb	%cl, %cl
	jnz	1b			/* If 0, no active partition found */
	jmp	boot_cdrom

found_active:
	movw	$str_press_key, %si
	call	message
next_second:
	movw	$str_dot, %si
	call	message
	decb	wait_count
	jz	boot_hard_disk
	xorb	%ah, %ah		/* Get system time */
	int	$0x1a
	movw	%dx, %di		/* %cx:%dx = number of clock ticks */
	addw	$19, %di		/* 19 ~= 18.2 Hz */
wait_key:
	movb	$1, %ah			/* Check for keystroke */
	int	$0x16
	jz	not_avail		/* ZF clear if keystroke available */
	xorb	%ah, %ah		/* Read key to flush keyboard buf */
	int	$0x16
	jmp	boot_cdrom
not_avail:
	xorb	%ah, %ah		/* Get system time */
	int	$0x1a
	cmpw	%dx, %di		/* Compare with saved time */
	jnz	wait_key
	jmp	next_second

boot_hard_disk:
	movw	$str_crlf, %si
	call	message
	cmpb	$1, mbr_loaded
	jz	1f
	movw	$0x0201, %ax		/* %al = number of sectors to read */
	movw	$BOOT_ADDR, %bx		/* %es:%bx = data buffer */
	movw	$0x0001, %cx		/* %ch = low 8 bits of cylinder no */
					/* %cl = high 2 bits of cyl no & */
					/*       sector number */
	movw	$0x0080, %dx		/* %dh = head number */
					/* %dl = disk number */
	int	$0x13			/* Read MBR into memory */
	jc	panic			/* CF set on error */
1:
	movw	%cs, %ax		/* Restore initial state */
	movw	%ax, %ds
	movw	%ax, %es
	movw	$0x0080, %dx		/* %dl = boot drive number */
	jmp	$0, $BOOT_ADDR		/* Jump to MBR! */
	jmp	panic			/* This should be never executed */
#endif /* !DISABLE_KEYPRESS */

boot_cdrom:
	movw	$str_banner, %si
	call	message

/* Read volume descriptor sectors until Primary decriptor found */
	movl	$VD_LBA, %eax
next_block:
	movb	$1, %dh			/* Number of sectors to read */
	movl	$PVD_ADDR, %ebx
	call	read_sectors
	cmpb	$VD_PRIMARY, (%bx)	/* Is it Primary Volume Descriptor? */
	jz	pvd_found
	incl	%eax
	cmpb	$VD_TERMINATOR, (%bx)
	jnz	next_block
	movw	$str_no_pvd, %si
	call	message
	jmp	panic

/* Read all of root directory */
pvd_found:
	movw	$PVD_ADDR+PVD_ROOT_DR, %bx
	movl	DR_EXTENT(%bx), %eax	/* LBA of the root directory */
	movl	DR_DATA_LEN(%bx), %edx
	shrl	$11, %edx		/* Convert to number of sectors */
	movb	%dl, %dh		/*  ... and load it to %dh */
	movl	$ROOTDIR_ADDR, %ebx
	call	read_sectors

/* Scan directory entries searching for /boot */
next_entry:
	cmpb	$0, DR_LEN(%bx)
	jz	last_entry
	movw	%bx, %si
	addw	$DR_NAME, %si
	movb	DR_NAME_LEN(%bx), %cl
	movw	$str_loader, %di
1:
	movb	(%si), %al
	cmpb	%al, (%di)
	jnz	fail
	incw	%si
	incw	%di
	decb	%cl
	jnz	1b
	jmp	load_loader
fail:
	addw	DR_LEN(%bx), %bx
	jmp	next_entry
last_entry:
	movw	$str_no_loader, %si
	call	message
	jmp	panic

/* Found /boot, read contents to 0x1000:0 */
load_loader:
	movl	DR_EXTENT(%bx), %eax
	movl	DR_DATA_LEN(%bx), %edx
	addl	$(BLOCK_SIZE-1), %edx	/* Convert file length to */
	shrl	$11, %edx		/*  ... number of sectors */
	movb	%dl, %dh
	movl	$LOADER_ADDR, %ebx
	call	read_sectors

/* Finally call into code of /boot */
	movl	$boot_params, %esi	/* Provide boot_params */
	xorl	%edx, %edx
	movb	boot_drive, %dl
	xorl	%ebx, %ebx		/* Zero sector number */
	lcall	$LOADER_ADDR/16, $0
	/* fall through on load failure */
panic:
	hlt
	jmp	panic

/*
 * Read disk sector(s) into memory
 *
 * %eax = LBA of starting sector
 * %ebx = buffer to store sectors
 * %dh = number of sectors to read
 *
 * Long transfers are split onto multiple 64k reads
 */
#define MAX_SECTORS (0x10000/BLOCK_SIZE)
read_sectors:
	pushal
	movl	%eax, edd_lba
	shrl	$4, %ebx		/* Convert buffer addr to seg:0 */
	movw	%bx, edd_segment
1:	movb	%dh, edd_nsecs
	cmpb	$MAX_SECTORS, %dh
	jle	2f			/* j if less than 64k */
	movb	$MAX_SECTORS, edd_nsecs	/* Read 32 sectors - 64k bytes */
2:	movb	boot_drive, %dl
	movw	$edd_packet, %si
read_again:
	movb	$0x42, %ah
	push	%dx			/* bios shouldn't kill %dh, but ... */
	int	$0x13
	pop	%dx			/* ... better safe than sorry! */
	jc	read_fail
	addw	$0x1000, edd_segment	/* Advance segment addr by 64k bytes */
	addl	$MAX_SECTORS, edd_lba	/* And sector number to match */
	sub	edd_nsecs, %dh		/* Number of sectors remaining */
	jnz	1b			
	popal
	ret

read_fail:
	cmpb	$ERROR_TIMEOUT, %ah
	jz	read_again
	movw	$str_read_error, %si
	call	message
	jmp	panic

#include <message.S>

edd_packet:
edd_len:	.word	16
edd_nsecs:	.word	0		/* Number of sectors to transfer */
edd_offset:	.word	0
edd_segment:	.word	0
edd_lba:	.quad	0

wait_count:	.byte	6
boot_drive:	.byte	0
mbr_loaded:	.byte	0

str_banner:	.ascii	"\r\nNetBSD/x86 cd9660 Primary Bootstrap"
str_crlf:	.asciz	"\r\n"
str_press_key:	.asciz	"\r\nPress any key to boot from CD"
str_dot:	.asciz	"."
str_read_error:	.asciz	"Can't read CD"
str_no_pvd:	.asciz	"Can't find Primary Volume Descriptor"
str_no_loader:	.asciz	"Can't find /minixboot"
str_loader:	.asciz	"MINIXBOOT.;1"

/* Used to calculate free bytes */
free_space = end - .

	. = start + BLOCK_SIZE
end:
