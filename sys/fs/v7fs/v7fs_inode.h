/*	$NetBSD: v7fs_inode.h,v 1.1 2011/06/27 11:52:25 uch Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#ifndef _V7FS_INODE_H_
#define	_V7FS_INODE_H_

/* Software implementation of inode. (memory image) */
struct v7fs_inode {
	v7fs_ino_t inode_number;	/* inode location */
	/* attr */
	uint16_t mode;
	int16_t nlink;
	int16_t uid;
	int16_t gid;
	v7fs_time_t atime;
	v7fs_time_t mtime;
	v7fs_time_t ctime;
	/* open mode */
	bool append_mode;

	v7fs_dev_t device;	/* for special file.(cdev, bdev) */
	/* payload. */
	v7fs_off_t filesize;	/* size of file (byte) */
	v7fs_daddr_t addr[V7FS_NADDR];	/* data block address list */
};

#define	v7fs_inode_filesize(i)	((i)->filesize)
#define	v7fs_inode_allocated(i)	((i)->mode)
#define	v7fs_inode_nlink(i)	((i)->nlink)
/* V7 original */
#define	v7fs_inode_isdir(i)	(((i)->mode & V7FS_IFMT) == V7FS_IFDIR)
#define	v7fs_inode_isfile(i)	(((i)->mode & V7FS_IFMT) == V7FS_IFREG)
#define	v7fs_inode_iscdev(i)	(((i)->mode & V7FS_IFMT) == V7FS_IFCHR)
#define	v7fs_inode_isbdev(i)	(((i)->mode & V7FS_IFMT) == V7FS_IFBLK)
/* 2BSD extension (implementation is different) */
#define	v7fs_inode_islnk(i)	(((i)->mode & V7FS_IFMT) == V7FSBSD_IFLNK)
#define	v7fs_inode_issock(i)	(((i)->mode & V7FS_IFMT) == V7FSBSD_IFSOCK)
/* NetBSD Extension */
#define	v7fs_inode_isfifo(i)	(((i)->mode & V7FS_IFMT) == V7FSBSD_IFFIFO)

__BEGIN_DECLS
/* Free inode access ops. */
int v7fs_inode_allocate(struct v7fs_self *, v7fs_ino_t *);
void v7fs_inode_deallocate(struct v7fs_self *, v7fs_ino_t);

/* Disk I/O ops. */
int v7fs_inode_load(struct v7fs_self *, struct v7fs_inode *, v7fs_ino_t);
int v7fs_inode_writeback(struct v7fs_self *, struct v7fs_inode *);
void v7fs_inode_setup_memory_image(const struct v7fs_self *,
    struct v7fs_inode *, struct v7fs_inode_diskimage *);

/* Check. */
int v7fs_inode_number_sanity(const struct v7fs_superblock *, v7fs_ino_t);

/* Util. */
void v7fs_inode_chmod(struct v7fs_inode *, v7fs_mode_t);
void v7fs_inode_dump(const struct v7fs_inode *);

/* Loop over all inode in ilist. */
int v7fs_ilist_foreach(struct v7fs_self *, int (*)(struct v7fs_self *, void *,
	struct v7fs_inode *, v7fs_ino_t), void *);
__END_DECLS
#endif /*!_V7FS_INODE_H_ */
