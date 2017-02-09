/*	$NetBSD: sysvbfs.h,v 1.10 2014/12/26 15:23:21 hannken Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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

#ifndef _FS_SYSVBFS_SYSVBFS_H_
#define	_FS_SYSVBFS_SYSVBFS_H_

struct bfs;
struct buf;

#include <fs/sysvbfs/sysvbfs_args.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

struct sysvbfs_node {
	struct genfs_node gnode;
	struct vnode *vnode;
	struct bfs_inode *inode;
	struct sysvbfs_mount *bmp;
	struct lockf *lockf;	/* advlock */
	daddr_t data_block;
	size_t size;
	int update_ctime;
	int update_atime;
	int update_mtime;
	int removed;
};

struct sysvbfs_mount {
	struct mount *mountp;
	struct vnode *devvp;		/* block device mounted vnode */
	struct bfs *bfs;
};

/* v-node ops. */
int sysvbfs_lookup(void *);
int sysvbfs_create(void *);
int sysvbfs_open(void *);
int sysvbfs_close(void *);
int sysvbfs_access(void *);
int sysvbfs_getattr(void *);
int sysvbfs_setattr(void *);
int sysvbfs_read(void *);
int sysvbfs_write(void *);
int sysvbfs_fsync(void *);
int sysvbfs_remove(void *);
int sysvbfs_rename(void *);
int sysvbfs_readdir(void *);
int sysvbfs_inactive(void *);
int sysvbfs_reclaim(void *);
int sysvbfs_bmap(void *);
int sysvbfs_strategy(void *);
int sysvbfs_print(void *);
int sysvbfs_advlock(void *);
int sysvbfs_pathconf(void *);

/* vfs ops. */
VFS_PROTOS(sysvbfs);

extern int (**sysvbfs_vnodeop_p)(void *);

/* genfs ops */
int sysvbfs_gop_alloc(struct vnode *, off_t, off_t, int, kauth_cred_t);
extern const struct genfs_ops sysvbfs_genfsops;

/* internal service */
int sysvbfs_update(struct vnode *, const struct timespec *,
    const struct timespec *, int);

#endif /* _FS_SYSVBFS_SYSVBFS_H_ */
