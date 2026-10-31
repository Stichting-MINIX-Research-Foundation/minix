/*	$NetBSD: ffs_bswap.c,v 1.1 2021/05/27 06:54:44 mrg Exp $	*/

/*
 * Copyright (c) 1998 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * This copy is a minimal version for libsa and it's ufs.c.  The unused
 * functions are removed, and additional swapping is performed on the
 * di_extb, di_db, and di_ib arrays.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_bswap.c,v 1.1 2021/05/27 06:54:44 mrg Exp $");

#include <sys/param.h>
#include <lib/libkern/libkern.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

static void
libsa_ffs_csumtotal_swap(const struct csum_total *o, struct csum_total *n)
{
	n->cs_ndir = bswap64(o->cs_ndir);
	n->cs_nbfree = bswap64(o->cs_nbfree);
	n->cs_nifree = bswap64(o->cs_nifree);
	n->cs_nffree = bswap64(o->cs_nffree);
}

void
ffs_sb_swap(const struct fs *o, struct fs *n)
{
	size_t i;
	const u_int32_t *o32;
	u_int32_t *n32;

	/*
	 * In order to avoid a lot of lines, as the first N fields (52)
	 * of the superblock up to fs_fmod are u_int32_t, we just loop
	 * here to convert them.
	 */
	o32 = (const u_int32_t *)o;
	n32 = (u_int32_t *)n;
	for (i = 0; i < offsetof(struct fs, fs_fmod) / sizeof(u_int32_t); i++)
		n32[i] = bswap32(o32[i]);

	n->fs_swuid = bswap64(o->fs_swuid);
	n->fs_cgrotor = bswap32(o->fs_cgrotor); /* Unused */
	n->fs_old_cpc = bswap32(o->fs_old_cpc);

	/* These fields overlap with a possible location for the
	 * historic FS_DYNAMICPOSTBLFMT postbl table, and with the
	 * first half of the historic FS_42POSTBLFMT postbl table.
	 */
	n->fs_maxbsize = bswap32(o->fs_maxbsize);
	/* XXX journal */
	n->fs_quota_magic = bswap32(o->fs_quota_magic);
	for (i = 0; i < MAXQUOTAS; i++)
		n->fs_quotafile[i] = bswap64(o->fs_quotafile[i]);
	n->fs_sblockloc = bswap64(o->fs_sblockloc);
	libsa_ffs_csumtotal_swap(&o->fs_cstotal, &n->fs_cstotal);
	n->fs_time = bswap64(o->fs_time);
	n->fs_size = bswap64(o->fs_size);
	n->fs_dsize = bswap64(o->fs_dsize);
	n->fs_csaddr = bswap64(o->fs_csaddr);
	n->fs_pendingblocks = bswap64(o->fs_pendingblocks);
	n->fs_pendinginodes = bswap32(o->fs_pendinginodes);

	/* These fields overlap with the second half of the
	 * historic FS_42POSTBLFMT postbl table
	 */
	for (i = 0; i < FSMAXSNAP; i++)
		n->fs_snapinum[i] = bswap32(o->fs_snapinum[i]);
	n->fs_avgfilesize = bswap32(o->fs_avgfilesize);
	n->fs_avgfpdir = bswap32(o->fs_avgfpdir);
	/* fs_sparecon[28] - ignore for now */
	n->fs_flags = bswap32(o->fs_flags);
	n->fs_contigsumsize = bswap32(o->fs_contigsumsize);
	n->fs_maxsymlinklen = bswap32(o->fs_maxsymlinklen);
	n->fs_old_inodefmt = bswap32(o->fs_old_inodefmt);
	n->fs_maxfilesize = bswap64(o->fs_maxfilesize);
	n->fs_qbmask = bswap64(o->fs_qbmask);
	n->fs_qfmask = bswap64(o->fs_qfmask);
	n->fs_state = bswap32(o->fs_state);
	n->fs_old_postblformat = bswap32(o->fs_old_postblformat);
	n->fs_old_nrpos = bswap32(o->fs_old_nrpos);
	n->fs_old_postbloff = bswap32(o->fs_old_postbloff);
	n->fs_old_rotbloff = bswap32(o->fs_old_rotbloff);

	n->fs_magic = bswap32(o->fs_magic);
}

void
ffs_dinode1_swap(struct ufs1_dinode *o, struct ufs1_dinode *n)
{
	size_t i;

	n->di_mode = bswap16(o->di_mode);
	n->di_nlink = bswap16(o->di_nlink);
	n->di_oldids[0] = bswap16(o->di_oldids[0]);
	n->di_oldids[1] = bswap16(o->di_oldids[1]);
	n->di_size = bswap64(o->di_size);
	n->di_atime = bswap32(o->di_atime);
	n->di_atimensec = bswap32(o->di_atimensec);
	n->di_mtime = bswap32(o->di_mtime);
	n->di_mtimensec = bswap32(o->di_mtimensec);
	n->di_ctime = bswap32(o->di_ctime);
	n->di_ctimensec = bswap32(o->di_ctimensec);
	/* Swap these here, unlike kernel version .*/
	for (i = 0; i < UFS_NDADDR; i++)
		n->di_db[i] = bswap32(o->di_db[i]);
	for (i = 0; i < UFS_NIADDR; i++)
		n->di_ib[i] = bswap32(o->di_ib[i]);
	n->di_flags = bswap32(o->di_flags);
	n->di_blocks = bswap32(o->di_blocks);
	n->di_gen = bswap32(o->di_gen);
	n->di_uid = bswap32(o->di_uid);
	n->di_gid = bswap32(o->di_gid);
}

void
ffs_dinode2_swap(struct ufs2_dinode *o, struct ufs2_dinode *n)
{
	size_t i;

	n->di_mode = bswap16(o->di_mode);
	n->di_nlink = bswap16(o->di_nlink);
	n->di_uid = bswap32(o->di_uid);
	n->di_gid = bswap32(o->di_gid);
	n->di_blksize = bswap32(o->di_blksize);
	n->di_size = bswap64(o->di_size);
	n->di_blocks = bswap64(o->di_blocks);
	n->di_atime = bswap64(o->di_atime);
	n->di_atimensec = bswap32(o->di_atimensec);
	n->di_mtime = bswap64(o->di_mtime);
	n->di_mtimensec = bswap32(o->di_mtimensec);
	n->di_ctime = bswap64(o->di_ctime);
	n->di_ctimensec = bswap32(o->di_ctimensec);
	n->di_birthtime = bswap64(o->di_birthtime);
	n->di_birthnsec = bswap32(o->di_birthnsec);
	n->di_gen = bswap32(o->di_gen);
	n->di_kernflags = bswap32(o->di_kernflags);
	n->di_flags = bswap32(o->di_flags);
	n->di_extsize = bswap32(o->di_extsize);
	/* Swap these here, unlike kernel version .*/
	for (i = 0; i < UFS_NXADDR; i++)
		n->di_extb[i] = bswap64(o->di_extb[i]);
	for (i = 0; i < UFS_NDADDR; i++)
		n->di_db[i] = bswap64(o->di_db[i]);
	for (i = 0; i < UFS_NIADDR; i++)
		n->di_ib[i] = bswap64(o->di_ib[i]);
}
