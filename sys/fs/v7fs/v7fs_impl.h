/*	$NetBSD: v7fs_impl.h,v 1.1 2011/06/27 11:52:25 uch Exp $	*/

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

/* V7FS implementation. */
#ifndef _V7FS_IMPL_H_
#define	_V7FS_IMPL_H_

#ifndef _KERNEL
#include <stdbool.h>
#include <assert.h>
#define	KDASSERT(x)	assert(x)
#endif

struct block_io_ops {
	void *cookie;
	bool (*drive)(void *, uint8_t);
	bool (*read)(void *, uint8_t *, daddr_t);
	bool (*read_n)(void *, uint8_t *, daddr_t, int);
	bool (*write)(void *, uint8_t *, daddr_t);
	bool (*write_n)(void *, uint8_t *, daddr_t, int);
};

#ifdef V7FS_EI
struct endian_conversion_ops {
	uint32_t (*conv32)(uint32_t);
	uint16_t (*conv16)(uint16_t);
	/* For daddr packing */
	v7fs_daddr_t (*conv24read)(uint8_t *);
	void (*conv24write)(v7fs_daddr_t, uint8_t *);
};
#endif
#ifdef _KERNEL
#define	V7FS_LOCK
#endif
#ifdef V7FS_LOCK
struct lock_ops {
	void *cookie;
	void (*lock)(void*);
	void (*unlock)(void *);
};
#define	SUPERB_LOCK(x)		((x)->sb_lock.lock((x)->sb_lock.cookie))
#define	SUPERB_UNLOCK(x)	((x)->sb_lock.unlock((x)->sb_lock.cookie))
#define	ILIST_LOCK(x)		((x)->ilist_lock.lock((x)->ilist_lock.cookie))
#define	ILIST_UNLOCK(x)		((x)->ilist_lock.unlock((x)->ilist_lock.cookie))
#define	MEM_LOCK(x)		((x)->mem_lock.lock((x)->mem_lock.cookie))
#define	MEM_UNLOCK(x)		((x)->mem_lock.unlock((x)->mem_lock.cookie))
#else /*V7FS_LOCK */
#define	SUPERB_LOCK(x)		((void)0)
#define	SUPERB_UNLOCK(x)	((void)0)
#define	ILIST_LOCK(x)		((void)0)
#define	ILIST_UNLOCK(x)		((void)0)
#define	MEM_LOCK(x)		((void)0)
#define	MEM_UNLOCK(x)		((void)0)
#endif /*V7FS_LOCK */

struct v7fs_stat {
	int32_t total_blocks;
	int32_t free_blocks;
	int32_t total_inode;
	int32_t free_inode;
	int32_t total_files;
};

struct v7fs_fileattr {
	int16_t uid;
	int16_t gid;
	v7fs_mode_t mode;
	v7fs_dev_t device;
	v7fs_time_t ctime;
	v7fs_time_t mtime;
	v7fs_time_t atime;
};

struct v7fs_self {
#define	V7FS_SELF_NSCRATCH	3
	uint8_t scratch[V7FS_SELF_NSCRATCH][V7FS_BSIZE];
	int scratch_free;	/* free block bitmap. */
	int scratch_remain;	/* for statistic */
	struct block_io_ops io;
#ifdef V7FS_EI
	struct endian_conversion_ops val;
#endif
#ifdef V7FS_LOCK
	/* in-core superblock access. (freeblock/freeinode) split? -uch */
	struct lock_ops sb_lock;
	struct lock_ops ilist_lock;	/* disk ilist access. */
	struct lock_ops mem_lock;	/* work memory allocation lock. */
#endif
	struct v7fs_superblock superblock;
	struct v7fs_stat stat;
	int endian;
};

struct v7fs_mount_device {
	union {
		void *vnode;	/* NetBSD kernel */
		int fd;		/* NetBSD newfs,fsck */
		const char *filename;/* misc test */
	} device;
	daddr_t sectors;	/*total size in sector. */
	int endian;
};

#define	V7FS_ITERATOR_BREAK	(-1)
#define	V7FS_ITERATOR_END	(-2)
#define	V7FS_ITERATOR_ERROR	(-3)
__BEGIN_DECLS
int v7fs_io_init(struct v7fs_self **, const struct v7fs_mount_device *, size_t);
void v7fs_io_fini(struct v7fs_self *);
void *scratch_read(struct v7fs_self *, daddr_t);
void scratch_free(struct v7fs_self *, void *);
int scratch_remain(const struct v7fs_self *);
__END_DECLS

#if 0
#define	V7FS_IO_DEBUG
#define	V7FS_SUPERBLOCK_DEBUG
#define	V7FS_DATABLOCK_DEBUG
#define	V7FS_INODE_DEBUG
#define	V7FS_DIRENT_DEBUG
#define	V7FS_FILE_DEBUG
#define	V7FS_VFSOPS_DEBUG
#define	V7FS_VNOPS_DEBUG
#endif

#endif /*!_V7FS_IMPL_H_ */
