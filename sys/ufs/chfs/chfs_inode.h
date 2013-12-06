/*	$NetBSD: chfs_inode.h,v 1.7 2013/01/22 09:39:15 dholland Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2010 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __CHFS_INODE_H__
#define __CHFS_INODE_H__

#ifdef _KERNEL
#include <sys/vnode.h>
#include <sys/stat.h>
#include <ufs/ufs/ufsmount.h>
#include <miscfs/genfs/genfs_node.h>
#endif /* _KERNEL */

#define CHFS_ROOTINO 2

/* chfs file types */
enum chtype {
	CHT_BLANK,	/* empty type */
	CHT_REG,	/* regular file */
	CHT_DIR,	/* directory */
	CHT_BLK,	/* block device */
	CHT_CHR,	/* character device */
	CHT_LNK,	/* link */
	CHT_SOCK,	/* socket */
	CHT_FIFO,	/* fifo */
	CHT_BAD		/* bad type */
};

/* these macros are needed because the compatibility */
#define CHTTOVT(ch_type)	(enum vtype)(ch_type)
#define VTTOCHT(v_type)		(enum chtype)(v_type)

/* vtype replaced with chtype, these are only for backward compatibility */
static const enum chtype iftocht_tab[16] = {
	CHT_BLANK, CHT_FIFO, CHT_CHR, CHT_BLANK,
	CHT_DIR, CHT_BLANK, CHT_BLK, CHT_BLANK,
	CHT_REG, CHT_BLANK, CHT_LNK, CHT_BLANK,
	CHT_SOCK, CHT_BLANK, CHT_BLANK, CHT_BAD,
};

#define	IFTOCHT(mode)	(iftocht_tab[((mode) & S_IFMT) >> 12])

#ifdef _KERNEL
struct chfs_inode
{
	struct genfs_node	gnode;
	kmutex_t inode_lock;		/* lock the fields of chfs_inode */

	LIST_ENTRY(chfs_inode) hash_entry;	/* hash chain */

	struct ufsmount *ump;		/* ufs mount - TODO we should remove it */
	struct chfs_mount *chmp;	/* chfs mount point - TODO we should remove it */

	struct vnode *vp;	/* vnode associated with this inode */
	ino_t ino;			/* vnode identifier number */
	
	struct vnode *devvp;	/* vnode for block I/O */
	dev_t dev;				/* device associated with the inode */
	
	struct chfs_vnode_cache *chvc;	/* vnode cache of this node */

	struct chfs_dirent *fd;			/* full dirent of this node */
	struct chfs_dirent_list dents;

	struct rb_tree fragtree;		/* fragtree of inode */

	uint64_t version;		/* version number */
	
	uint32_t mode;			/* mode */
	enum chtype ch_type;	/* chfs file type */
	uint64_t size;			/* file byte count */
	uint64_t write_size;	/* increasing while write the file out to the flash */
	uint32_t uid;			/* file owner */
	uint32_t gid;			/* file group */
	uint32_t atime;			/* access time */
	uint32_t mtime;			/* modify time */
	uint32_t ctime;			/* creation time */
	
	uint32_t iflag;			/* flags, see below */
	uint32_t flags;			/* status flags (chflags) */

	dev_t rdev;				/* used if type is VCHR or VBLK or VFIFO*/
	char *target;			/* used if type is VLNK */
};

/* These flags are kept in chfs_inode->iflag. */
#define	IN_ACCESS	0x0001		/* Access time update request. */
#define	IN_CHANGE	0x0002		/* Inode change time update request. */
#define	IN_UPDATE	0x0004		/* Inode was written to; update mtime. */
#define	IN_MODIFY	0x2000		/* Modification time update request. */
#define	IN_MODIFIED	0x0008		/* Inode has been modified. */
#define	IN_ACCESSED	0x0010		/* Inode has been accessed. */
#define	IN_RENAME	0x0020		/* Inode is being renamed. */
#define	IN_SHLOCK	0x0040		/* File has shared lock. */
#define	IN_EXLOCK	0x0080		/* File has exclusive lock. */
#define	IN_CLEANING	0x0100		/* LFS: file is being cleaned */
#define	IN_ADIROP	0x0200		/* LFS: dirop in progress */
#define	IN_SPACECOUNTED	0x0400		/* Blocks to be freed in free count. */
#define	IN_PAGING       0x1000		/* LFS: file is on paging queue */


#ifdef VTOI
# undef VTOI
#endif
#ifdef ITOV
# undef ITOV
#endif

/* struct vnode to struct chfs_inode */
#define	VTOI(vp)	((struct chfs_inode *)(vp)->v_data)
/* struct chfs_inode to struct vnode */
#define	ITOV(ip)	((ip)->vp)

/* XXX copied from ufs_dinode.h and should not be duplicated here */
#define	UFS_NDADDR	12		/* Direct addresses in inode. */

/* XXX this should not be duplicated here */
#define	UFS_ROOTINO	((ino_t)2)

/* File permissions. */
#define	IEXEC		0000100		/* Executable. */
#define	IWRITE		0000200		/* Writable. */
#define	IREAD		0000400		/* Readable. */
#define	ISVTX		0001000		/* Sticky bit. */
#define	ISGID		0002000		/* Set-gid. */
#define	ISUID		0004000		/* Set-uid. */

/* File types. */
#define	IFMT		0170000		/* Mask of file type. */
#define	IFIFO		0010000		/* Named pipe (fifo). */
#define	IFCHR		0020000		/* Character device. */
#define	IFDIR		0040000		/* Directory file. */
#define	IFBLK		0060000		/* Block device. */
#define	IFREG		0100000		/* Regular file. */
#define	IFLNK		0120000		/* Symbolic link. */
#define	IFSOCK		0140000		/* UNIX domain socket. */
#define	IFWHT		0160000		/* Whiteout. */

#endif /* _KERNEL */
#endif /* __CHFS_INODE_H__ */
