/* $NetBSD: biosvbe.S,v 1.3 2011/02/20 22:03:13 jakllsch Exp $ */

/*-
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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

#include <machine/asm.h>

	.text

/*
 * VESA BIOS Extensions routines
 */

/*
 * Function 00h - Return VBE Controller Information
 *
 * int biosvbe_info(struct vbeinfoblock *)
 * return: VBE call status
 */
ENTRY(biosvbe_info)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %edi	/* vbe info block address*/

	call	_C_LABEL(prot_to_real)
	.code16

	push	%es

	push	%di
	shrl	$4, %edi
	mov	%ds, %ax
	add	%di, %ax
	mov	%ax, %es
	pop	%di
	and	$0xf, %di	/* mode info block address now in es:di */

	movw	$0x4f00, %ax	/* get vbe info block */
	int	$0x10

	pop	%es

	calll	_C_LABEL(real_to_prot)
	.code32

	andl	$0xffff,%eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
 * Function 01h - Return VBE Mode Information
 *
 * int biosvbe_get_mode_info(int mode, struct modeinfoblock *mi)
 * return: VBE call status
 */
ENTRY(biosvbe_get_mode_info)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %ecx	/* mode number */
	movl	12(%ebp), %edi	/* mode info block address */

	call	_C_LABEL(prot_to_real)
	.code16

	push	%es

	push	%di
	shrl	$4, %edi
	mov	%ds, %ax
	add	%di, %ax
	mov	%ax, %es
	pop	%di
	and	$0xf, %di	/* mode info block address now in es:di */

	movw	$0x4f01, %ax	/* get mode info block */
	int	$0x10

	pop	%es

	calll	_C_LABEL(real_to_prot)
	.code32

	andl	$0xffff,%eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
 * Function 02h - Set VBE Mode
 *
 * int biosvbe_set_mode(int mode)
 * return: VBE call status
 */
ENTRY(biosvbe_set_mode)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %ebx	/* mode number */
	orl	$0x4000, %ebx

	call	_C_LABEL(prot_to_real)
	.code16

	movw	$0x4f02, %ax	/* set mode */
	int	$0x10

	calll	_C_LABEL(real_to_prot)
	.code32

	andl	$0xffff,%eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
 * Function 08h - Set/Get DAC Palette Format
 *
 * int biosvbe_palette_format(int format)
 * return: VBE call status
 */
ENTRY(biosvbe_palette_format)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %ebx	/* mode number */

	call	_C_LABEL(prot_to_real)
	.code16

	movw	$0x4f08, %ax	/* get/set palette format */
	int	$0x10

	calll	_C_LABEL(real_to_prot)
	.code32

	andl	$0xffff,%eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
 * Function 09h - Set/Get Palette Data
 *
 * int biosvbe_palette_data(int mode, int reg, struct paletteentry *)
 * return: VBE call status
 */
ENTRY(biosvbe_palette_data)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %ebx	/* mode number */
	movl	12(%ebp), %edx	/* register */
	movl	16(%ebp), %edi	/* palette entry address */
	movl	$1, %ecx	/* # palette entries to update */

	call	_C_LABEL(prot_to_real)
	.code16

	push	%es

	push	%di
	shrl	$4, %edi
	mov	%ds, %ax
	add	%di, %ax
	mov	%ax, %es
	pop	%di
	and	$0xf, %di	/* palette entry address now in es:di */

	movw	$0x4f09, %ax	/* get/set palette entry */
	int	$0x10

	pop	%es

	calll	_C_LABEL(real_to_prot)
	.code32

	andl	$0xffff,%eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
 * Function 15h BL=00h - Report VBE/DDC Capabilities
 *
 * int biosvbe_ddc_caps(void)
 * return: VBE/DDC capabilities
 */
ENTRY(biosvbe_ddc_caps)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	call	_C_LABEL(prot_to_real)
	.code16

	pushw	%es

	xorw	%di, %di
	movw	%di, %es	/* es:di == 0:0 */

	movw	$0x4f15, %ax	/* display identification extensions */
	mov	$0x00, %bx	/* report DDC capabilities */
	mov	$0x00, %cx	/* controller unit number (00h = primary) */
	int	$0x10

	popw	%es

	calll	_C_LABEL(real_to_prot)
	.code32

	movl	%eax,%ecx
	movl	$0x0000,%eax
	andl	$0xffff,%ecx
	cmpl	$0x004f,%ecx
	jne	1f
	andl	$0xffff,%ebx
	movl	%ebx,%eax
1:

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

/*
 * Function 15h BL=01h - Read EDID
 *
 * int biosvbe_ddc_read_edid(int blockno, void *buf)
 * return: VBE call status
 */
ENTRY(biosvbe_ddc_read_edid)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	push	%esi
	push	%edi

	movl	8(%ebp), %edx	/* EDID block number */
	movl	12(%ebp), %edi	/* EDID block address */

	call	_C_LABEL(prot_to_real)
	.code16

	push	%es

	push	%di
	shrl	$4, %edi
	mov	%ds, %ax
	add	%di, %ax
	mov	%ax, %es
	pop	%di
	and	$0xf, %di	/* EDID block address now in es:di */

	movw	$0x4f15, %ax	/* display identification extensions */
	mov	$0x01, %bx	/* read EDID */
	mov	$0x00, %cx	/* controller unit number (00h = primary) */
	int	$0x10

	pop	%es

	calll	_C_LABEL(real_to_prot)
	.code32

	andl	$0xffff,%eax

	pop	%edi
	pop	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret

