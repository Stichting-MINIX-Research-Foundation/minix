/*	$NetBSD: ext2fs_bswap.c,v 1.24 2016/08/20 19:47:44 jdolecek Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: ext2fs_bswap.c,v 1.24 2016/08/20 19:47:44 jdolecek Exp $");

#include <sys/types.h>
#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_dinode.h>

#if defined(_KERNEL)
#include <sys/systm.h>
#else
#include <string.h>
#endif

/* These functions are only needed if native byte order is not little endian */
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
	new->e4fs_want_extra_isize =	bswap16(old->e4fs_want_extra_isize);
}

void
e2fs_i_bswap(struct ext2fs_dinode *old, struct ext2fs_dinode *new, size_t isize)
{
	/* preserve non-swapped and unused fields */ 
	memcpy(new, old, isize);

	/* swap what needs to be swapped */
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
	new->e2di_version	=	bswap32(old->e2di_version);
	new->e2di_gen		=	bswap32(old->e2di_gen);
	new->e2di_facl		=	bswap32(old->e2di_facl);
	new->e2di_size_high	=	bswap32(old->e2di_size_high);
	new->e2di_nblock_high	=	bswap16(old->e2di_nblock_high);
	new->e2di_facl_high	=	bswap16(old->e2di_facl_high);
	new->e2di_uid_high	=	bswap16(old->e2di_uid_high);
	new->e2di_gid_high	=	bswap16(old->e2di_gid_high);
	new->e2di_checksum_low  = 	bswap16(old->e2di_checksum_low);

	/*
	 * Following fields are only supported for inode sizes bigger
	 * than the old ext2 one
	 */
	if (isize == EXT2_REV0_DINODE_SIZE)
		return;

	new->e2di_extra_isize   = bswap16(old->e2di_extra_isize);
	new->e2di_checksum_high = bswap16(old->e2di_checksum_high);

	/* Following fields are ext4, might not be actually present */
	if (EXT2_DINODE_FITS(new, e2di_ctime_extra, isize))
		new->e2di_ctime_extra   = bswap32(old->e2di_ctime_extra);
	if (EXT2_DINODE_FITS(new, e2di_mtime_extra, isize))
		new->e2di_mtime_extra	= bswap32(old->e2di_mtime_extra);
	if (EXT2_DINODE_FITS(new, e2di_atime_extra, isize))
		new->e2di_atime_extra	= bswap32(old->e2di_atime_extra);
	if (EXT2_DINODE_FITS(new, e2di_crtime, isize))
		new->e2di_crtime	= bswap32(old->e2di_crtime);
	if (EXT2_DINODE_FITS(new, e2di_crtime_extra, isize))
		new->e2di_crtime_extra	= bswap32(old->e2di_crtime_extra);
	if (EXT2_DINODE_FITS(new, e2di_version_high, isize))
		new->e2di_version_high	= bswap32(old->e2di_version_high);
	if (EXT2_DINODE_FITS(new, e2di_projid, isize))
		new->e2di_projid	= bswap32(old->e2di_projid);
}
#endif
