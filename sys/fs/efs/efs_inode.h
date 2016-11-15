/*	$NetBSD: efs_inode.h,v 1.1 2007/06/29 23:30:29 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble <rumble@ephemeral.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _FS_EFS_EFS_INODE_H_
#define _FS_EFS_EFS_INODE_H_

#include <fs/efs/efs_subr.h>

/*
 * The efs_inode structure represents an in-core inode. It contains meta-data
 * corresponding directly to the efs_dinode structure in host byte order and
 * with native NetBSD flags and fields, a copy of the on-disk dinode structure,
 * as well as other bookkeeping information.
 */
struct efs_inode {
	struct genfs_node	ei_gnode;
	struct lockf	       *ei_lockf;	/* advisory lock */
	ino_t			ei_number;	/* inode number */
	dev_t			ei_dev;		/* associated device */
	struct vnode	       *ei_vp;		/* associated vnode */
	LIST_ENTRY(efs_inode)	ei_hash;	/* inode hash chain */

	/*
	 * Host-ordered on-disk fields with native NetBSD types and flags.
	 */
	uint16_t        	ei_mode;        /* file type and permissions */
	int16_t         	ei_nlink;       /* link count (minimum 2) */
	uid_t			ei_uid;         /* user ID */
	gid_t			ei_gid;         /* group ID */
	int32_t         	ei_size;        /* file size (in bytes) */
	time_t          	ei_atime;       /* file access time */
	time_t          	ei_mtime;       /* file modification time */
	time_t          	ei_ctime;       /* inode modification time */
	int32_t         	ei_gen;         /* inode generation number */
	int16_t         	ei_numextents;  /* number of extents in file */
	uint8_t         	ei_version;     /* inode version */

	/*
	 * Copy of the on-disk inode structure, in EFS native format and
	 * endianness.
	 */
	struct efs_dinode 	ei_di;
};

#define EFS_VTOI(vp)	((struct efs_inode *)(vp)->v_data)
#define EFS_ITOV(eip)	((struct vnode *)(eip)->ei_vp)

/*
 * File handle. The first two fields must match struct fid (see sys/fstypes.h).
 */
struct efs_fid {
	unsigned short	ef_len;			/* length of data in bytes */
	unsigned short	ef_pad;			/* compat: historic align */
	int32_t		ef_ino;			/* inode number */
	int32_t		ef_gen;			/* inode generation number */
};

#endif /* !_FS_EFS_EFS_INODE_H_ */
