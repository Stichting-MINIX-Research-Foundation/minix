/*	$NetBSD: ext2fs_bswap.c,v 1.16 2009/10/19 18:41:17 bouyer Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_bswap.c,v 1.16 2009/10/19 18:41:17 bouyer Exp $");

#include <sys/types.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dinode.h>

#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <string.h>
#endif

/* These functions are only needed if native byte order is not big endian */
#if BYTE_ORDER == BIG_ENDIAN
void
e2fs_sb_bswap(struct ext2fs *old, struct ext2fs *new)
{

	/* preserve unused fields */
	memcpy(new, old, sizeof(struct ext2fs));
	new->e2fs_icount	=	bswap32(old->e2fs_icount);
	new->e2fs_bcount	=	bswap32(old->e2fs_bcount);
	new->e2fs_rbcount	=	bswap32(old->e2fs_rbcount);
	new->e2fs_fbcount	=	bswap32(old->e2fs_fbcount);
	new->e2fs_ficount	=	bswap32(old->e2fs_ficount);
	new->e2fs_first_dblock	=	bswap32(old->e2fs_first_dblock);
	new->e2fs_log_bsize	=	bswap32(old->e2fs_log_bsize);
	new->e2fs_fsize		=	bswap32(old->e2fs_fsize);
	new->e2fs_bpg		=	bswap32(old->e2fs_bpg);
	new->e2fs_fpg		=	bswap32(old->e2fs_fpg);
	new->e2fs_ipg		=	bswap32(old->e2fs_ipg);
	new->e2fs_mtime		=	bswap32(old->e2fs_mtime);
	new->e2fs_wtime		=	bswap32(old->e2fs_wtime);
	new->e2fs_mnt_count	=	bswap16(old->e2fs_mnt_count);
	new->e2fs_max_mnt_count	=	bswap16(old->e2fs_max_mnt_count);
	new->e2fs_magic		=	bswap16(old->e2fs_magic);
	new->e2fs_state		=	bswap16(old->e2fs_state);
	new->e2fs_beh		=	bswap16(old->e2fs_beh);
	new->e2fs_minrev	=	bswap16(old->e2fs_minrev);
	new->e2fs_lastfsck	=	bswap32(old->e2fs_lastfsck);
	new->e2fs_fsckintv	=	bswap32(old->e2fs_fsckintv);
	new->e2fs_creator	=	bswap32(old->e2fs_creator);
	new->e2fs_rev		=	bswap32(old->e2fs_rev);
	new->e2fs_ruid		=	bswap16(old->e2fs_ruid);
	new->e2fs_rgid		=	bswap16(old->e2fs_rgid);
	new->e2fs_first_ino	=	bswap32(old->e2fs_first_ino);
	new->e2fs_inode_size	=	bswap16(old->e2fs_inode_size);
	new->e2fs_block_group_nr =	bswap16(old->e2fs_block_group_nr);
	new->e2fs_features_compat =	bswap32(old->e2fs_features_compat);
	new->e2fs_features_incompat =	bswap32(old->e2fs_features_incompat);
	new->e2fs_features_rocompat =	bswap32(old->e2fs_features_rocompat);
	new->e2fs_algo		=	bswap32(old->e2fs_algo);
	new->e2fs_reserved_ngdb	=	bswap16(old->e2fs_reserved_ngdb);
}

void e2fs_cg_bswap(struct ext2_gd *old, struct ext2_gd *new, int size)
{
	int i;

	for (i = 0; i < (size / (int)sizeof(struct  ext2_gd)); i++) {
		new[i].ext2bgd_b_bitmap	= bswap32(old[i].ext2bgd_b_bitmap);
		new[i].ext2bgd_i_bitmap	= bswap32(old[i].ext2bgd_i_bitmap);
		new[i].ext2bgd_i_tables	= bswap32(old[i].ext2bgd_i_tables);
		new[i].ext2bgd_nbfree	= bswap16(old[i].ext2bgd_nbfree);
		new[i].ext2bgd_nifree	= bswap16(old[i].ext2bgd_nifree);
		new[i].ext2bgd_ndirs	= bswap16(old[i].ext2bgd_ndirs);
	}
}

void e2fs_i_bswap(struct ext2fs_dinode *old, struct ext2fs_dinode *new)
{

	new->e2di_mode		=	bswap16(old->e2di_mode);
	new->e2di_uid		=	bswap16(old->e2di_uid);
	new->e2di_gid		=	bswap16(old->e2di_gid);
	new->e2di_nlink		=	bswap16(old->e2di_nlink);
	new->e2di_size		=	bswap32(old->e2di_size);
	new->e2di_atime		=	bswap32(old->e2di_atime);
	new->e2di_ctime		=	bswap32(old->e2di_ctime);
	new->e2di_mtime		=	bswap32(old->e2di_mtime);
	new->e2di_dtime		=	bswap32(old->e2di_dtime);
	new->e2di_nblock	=	bswap32(old->e2di_nblock);
	new->e2di_flags		=	bswap32(old->e2di_flags);
	new->e2di_gen		=	bswap32(old->e2di_gen);
	new->e2di_facl		=	bswap32(old->e2di_facl);
	new->e2di_dacl		=	bswap32(old->e2di_dacl);
	new->e2di_faddr		=	bswap32(old->e2di_faddr);
	new->e2di_uid_high	=	bswap16(old->e2di_uid_high);
	new->e2di_gid_high	=	bswap16(old->e2di_gid_high);
	memcpy(&new->e2di_blocks[0], &old->e2di_blocks[0],
	    (NDADDR + NIADDR) * sizeof(uint32_t));
}
#endif
