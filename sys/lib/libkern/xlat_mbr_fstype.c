/*	$NetBSD: xlat_mbr_fstype.c,v 1.9 2013/03/02 22:04:06 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0,"$NetBSD: xlat_mbr_fstype.c,v 1.9 2013/03/02 22:04:06 christos Exp $");


#include <sys/disklabel.h>
#include <sys/bootblock.h>

int
xlat_mbr_fstype(int mbr_type)
{
	static const struct ptn_types {
		uint8_t	mbr_type;
		uint8_t	netbsd_type;
	} ptn_types[] = {
		{ MBR_PTYPE_386BSD,	FS_BSDFFS },
		{ MBR_PTYPE_APPLE_UFS,	FS_APPLEUFS },
		{ MBR_PTYPE_FAT12,	FS_MSDOS },
		{ MBR_PTYPE_FAT16B,	FS_MSDOS },
		{ MBR_PTYPE_FAT16L,	FS_MSDOS },
		{ MBR_PTYPE_FAT16S,	FS_MSDOS },
		{ MBR_PTYPE_FAT32,	FS_MSDOS },
		{ MBR_PTYPE_FAT32L,	FS_MSDOS },
		{ MBR_PTYPE_LNXEXT2,	FS_EX2FS },
		{ MBR_PTYPE_LNXSWAP,	FS_SWAP },
		{ MBR_PTYPE_NETBSD,	FS_BSDFFS },
		{ MBR_PTYPE_NTFS,	FS_NTFS },
		{ MBR_PTYPE_MINIX_14B,	FS_MINIXFS3 },
		{ MBR_PTYPE_OPENBSD,	FS_BSDFFS },
		{ 0,			FS_OTHER }
	};
	const struct ptn_types *pt;
	for (pt = ptn_types; pt->mbr_type != 0; pt++)
		if (mbr_type == pt->mbr_type)
			break;
	return pt->netbsd_type;
}
