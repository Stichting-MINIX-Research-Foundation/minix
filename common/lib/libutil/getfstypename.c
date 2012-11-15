/*	$NetBSD: getfstypename.c,v 1.8 2012/04/07 16:28:59 christos Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#ifndef _STANDALONE
# include <sys/cdefs.h>
# ifndef _KERNEL
#  if !defined(lint)
__RCSID("$NetBSD: getfstypename.c,v 1.8 2012/04/07 16:28:59 christos Exp $");
#  endif
# else
__KERNEL_RCSID(0, "$NetBSD: getfstypename.c,v 1.8 2012/04/07 16:28:59 christos Exp $");
# endif /* _KERNEL */

# define FSTYPE_ENUMNAME fstype_enum
# include <sys/types.h>
# include <sys/disk.h>
# include <sys/disklabel.h>
# ifndef _KERNEL
#  include <util.h>
# endif

const char *
getfstypename(int fstype)
{
	/*
	 * The cast is so that the compiler can check that we
	 * cover all the enum values
	 */
	switch ((enum fstype_enum)fstype) {
	case FS_UNUSED:
		return DKW_PTYPE_UNUSED;
	case FS_SWAP:
		return DKW_PTYPE_SWAP;
	case FS_V6:
		return DKW_PTYPE_V6;
	case FS_V7:
		return DKW_PTYPE_V7;
	case FS_SYSV:
		return DKW_PTYPE_SYSV;
	case FS_V71K:
		return DKW_PTYPE_V71K;
	case FS_V8:
		return DKW_PTYPE_V8;
	case FS_BSDFFS:
		return DKW_PTYPE_FFS;
	case FS_MSDOS:
		return DKW_PTYPE_FAT;
	case FS_BSDLFS:
		return DKW_PTYPE_LFS;
	case FS_OTHER:
		return DKW_PTYPE_OTHER;
	case FS_HPFS:
		return DKW_PTYPE_HPFS;
	case FS_ISO9660:
		return DKW_PTYPE_ISO9660;
	case FS_BOOT:
		return DKW_PTYPE_BOOT;
	case FS_ADOS:
		return DKW_PTYPE_AMIGADOS;
	case FS_HFS:
		return DKW_PTYPE_APPLEHFS;
	case FS_FILECORE:
		return DKW_PTYPE_FILECORE;
	case FS_EX2FS:
		return DKW_PTYPE_EXT2FS;
	case FS_NTFS:
		return DKW_PTYPE_NTFS;
	case FS_RAID:
		return DKW_PTYPE_RAIDFRAME;
	case FS_CCD:
		return DKW_PTYPE_CCD;
	case FS_JFS2:
		return DKW_PTYPE_JFS2;
	case FS_APPLEUFS:
		return DKW_PTYPE_APPLEUFS;
	case FS_VINUM:
		return DKW_PTYPE_VINUM;
	case FS_UDF:
		return DKW_PTYPE_UDF;
	case FS_SYSVBFS:
		return DKW_PTYPE_SYSVBFS;
	case FS_EFS:
		return DKW_PTYPE_EFS;
	case FS_NILFS:
		return DKW_PTYPE_NILFS;
	case FS_CGD:
		return DKW_PTYPE_CGD;
	case FSMAXTYPES:
		return DKW_PTYPE_UNKNOWN;
	case FS_MINIXFS3:
		return DKW_PTYPE_MINIXFS3;
	}
	/* Stupid gcc, should know it is impossible to get here */
	/*NOTREACHED*/
	return DKW_PTYPE_UNKNOWN;
}
#endif /* !_STANDALONE */
