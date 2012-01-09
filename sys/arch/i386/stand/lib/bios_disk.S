/*	$NetBSD: bios_disk.S,v 1.21 2011/06/16 13:27:59 joerg Exp $	*/

/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* extracted from netbsd:sys/arch/i386/boot/bios.S */

#include <machine/asm.h>

/*
 * BIOS call "INT 0x13 Function 0x0" to reset the disk subsystem
 *	Call with	%ah = 0x0
 *			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
 *	Return:
 *			%al = 0x0 on success; err code on failure
 */
ENTRY(biosdisk_reset)
	pusha

	movb	%al, %dl	# device

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$0x0, %ah	# subfunction
	int	$0x13
	setc	%bl
	movb	%ah, %bh	# save error code

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movzwl	%bx, %eax	# return value in %eax
	movl	%eax, 28(%esp)

	popa
	ret

/*
 * BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
 *	Call with	%ah = 0x2
 *			%al = number of sectors
 *			%ch = cylinder
 *			%cl = sector
 *			%dh = head
 *			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
 *			%es:%bx = segment:offset of buffer
 *	Return:
 *			%al = 0x0 on success; err code on failure
 *
 * biosdisk_read(dev, cyl, head, sect, count, buff_addr);
 *
 *  Note: On failure, you must reset the disk with biosdisk_reset() before
 *        sending another command.
 */
ENTRY(biosdisk_read)
	pusha

	movb	44(%esp), %dh
	movw	40(%esp), %cx
	xchgb	%ch, %cl	# cylinder; the highest 2 bits of cyl is in %cl
	rorb	$2, %cl
	movb	48(%esp), %al
	orb	%al, %cl
	incb	%cl		# sector; sec starts from 1, not 0
	movb	36(%esp), %dl	# device
	movl	56(%esp), %ebx	# buffer address (may be >64k)
	movb	52(%esp), %al	# number of sectors

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	push	%bx
	shrl	$4, %ebx	# max segment
	mov	%ds, %si
	add	%si, %bx
	mov	%bx, %es	# %es:%bx now valid buffer address
	pop	%bx
	and	$0xf, %bx	# and min offset - to avoid overrun

	movb	$0x2, %ah	# subfunction
	int	$0x13
	setc	%al		# error code is in %ah

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movl	%eax, 28(%esp)

	popa
	ret

/*
 * biosdisk_getinfo(int dev):  return a word that represents the
 *	max number of sectors, heads and cylinders for this device
 */
ENTRY(biosdisk_getinfo)
	push	%es
	pusha

	movb	%al, %dl		# diskinfo(drive #)

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	push	%dx			# save drive #
	movb	$0x08, %ah		# ask for disk info
	int	$0x13
	pop	%bx			# restore drive #
	jnc	ok

	testb	$0x80, %bl		# is it a hard disk?
	jnz	ok

	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.  Initialize all the registers for
	 * documentation, although we only need head and sector counts.
	 */
	xorw	%ax, %ax		# set status to success
#	movb	%ah, %bh		# %bh = 0
#	movb	$2, %bl			# %bl bits 0-3 = drive type, 2 = 1.2M
	movb	$79, %ch		# max track
	movb	$15, %cl		# max sector
	movb	$1, %dh			# max head
#	movb	$1, %dl			# # floppy drives installed
	# es:di = parameter table
	# carry = 0

ok:
	calll	_C_LABEL(real_to_prot)	# back to protected mode
	.code32

	/* form a longword representing all this gunk */
	shrl	$8, %eax		# clear unnecessary bits
	shll	$24, %eax
	shll	$16, %ecx		# do the same for %ecx
	shrl	$8, %ecx
	movb	%dh, %cl		# max head
	orl	%ecx, %eax		# return value in %eax
	movl	%eax, 28(%esp)

	popa
	pop	%es
	ret

/*
 * int biosdisk_int13ext(int dev):
 *	check for availibility of int13 extensions.
 */
ENTRY(biosdisk_int13ext)
	pusha

	movb	%al, %dl		# drive #
	movw	$0x55aa, %bx

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	movb	$0x41, %ah		# ask for disk info
	int	$0x13
	setnc	%dl

	calll	_C_LABEL(real_to_prot)	# switch back
	.code32

	movzbl	%dl, %eax		# return value in %eax

	cmpw	$0xaa55, %bx
	sete	%dl
	andb	%dl, %al

	andb	%cl, %al
	movl	%eax, 28(%esp)

	popa
	ret

/*
 * BIOS call "INT 0x13 Function 0x42" to read sectors from disk into memory
 *	Call with	%ah = 0x42
 *			%ds:%si = parameter block (data buffer address
 *				must be a real mode physical address).
 *			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
 *	Return:
 *			%al = 0x0 on success; err code on failure
 */
ENTRY(biosdisk_extread)
	pusha

	movl	%edx, %esi	# parameter block
	movb	%al, %dl	# device

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	push	%ds
	movl	%esi, %eax
	shrl	$4, %eax
	movw	%ds, %bx
	addw	%bx, %ax
	movw	%ax, %ds
	andw	$0xf, %si

	movb	$0x42, %ah	# subfunction
	int	$0x13
	setc	%bl
	movb	%ah, %bh	# save error code
	pop	%ds

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movzwl	%bx, %eax	# return value in %eax
	movl	%eax, 28(%esp)

	popa
	ret

ENTRY(biosdisk_getextinfo)
	pusha

	movl	%edx, %esi	# parameter block
	movb	%al, %dl	# device

	call	_C_LABEL(prot_to_real)	# enter real mode
	.code16

	push	%ds
	movl	%esi, %eax
	shrl	$4, %eax
	andw	$0xf, %si
	movw	%ds, %bx
	addw	%bx, %ax
	movw	%ax, %ds

	movb	$0x48, %ah	# subfunction
	int	$0x13
	setc	%bl
	pop	%ds

	calll	_C_LABEL(real_to_prot) # back to protected mode
	.code32

	movzbl	%bl, %eax	# return value in %eax
	movl	%eax, 28(%esp)

	popa
	ret
