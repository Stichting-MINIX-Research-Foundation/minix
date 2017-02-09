/*	$NetBSD: specdev.h,v 1.44 2015/06/23 10:42:35 hannken Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
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

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)specdev.h	8.6 (Berkeley) 5/21/95
 */

#ifndef _MISCFS_SPECFS_SPECDEV_H_
#define _MISCFS_SPECFS_SPECDEV_H_

#include <sys/mutex.h>
#include <sys/vnode.h>

typedef struct specnode {
	vnode_t		*sn_next;
	struct specdev	*sn_dev;
	u_int		sn_opencnt;
	dev_t		sn_rdev;
	bool		sn_gone;
} specnode_t;

typedef struct specdev {
	struct mount	*sd_mountpoint;
	struct lockf	*sd_lockf;
	vnode_t		*sd_bdevvp;
	u_int		sd_opencnt;
	u_int		sd_refcnt;
	dev_t		sd_rdev;
} specdev_t;

/*
 * Exported shorthand
 */
#define v_specnext	v_specnode->sn_next
#define v_rdev		v_specnode->sn_rdev
#define v_speclockf	v_specnode->sn_dev->sd_lockf

/*
 * Special device management
 */
void	spec_node_init(vnode_t *, dev_t);
void	spec_node_destroy(vnode_t *);
int	spec_node_lookup_by_dev(enum vtype, dev_t, vnode_t **);
int	spec_node_lookup_by_mount(struct mount *, vnode_t **);
struct mount *spec_node_getmountedfs(vnode_t *);
void	spec_node_setmountedfs(vnode_t *, struct mount *);
void	spec_node_revoke(vnode_t *);

/*
 * Prototypes for special file operations on vnodes.
 */
extern	int (**spec_vnodeop_p)(void *);
struct	nameidata;
struct	componentname;
struct	flock;
struct	buf;
struct	uio;

int	spec_lookup(void *);
#define	spec_create	genfs_badop
#define	spec_whiteout	genfs_badop
#define	spec_mknod	genfs_badop
int	spec_open(void *);
int	spec_close(void *);
#define	spec_access	genfs_ebadf
#define	spec_getattr	genfs_ebadf
#define	spec_setattr	genfs_ebadf
int	spec_read(void *);
int	spec_write(void *);
#define spec_fallocate	genfs_eopnotsupp
int	spec_fdiscard(void *);
#define spec_fcntl	genfs_fcntl
int	spec_ioctl(void *);
int	spec_poll(void *);
int	spec_kqfilter(void *);
#define spec_revoke	genfs_revoke
int	spec_mmap(void *);
int	spec_fsync(void *);
#define	spec_seek	genfs_nullop		/* XXX should query device */
#define	spec_remove	genfs_badop
#define	spec_link	genfs_badop
#define	spec_rename	genfs_badop
#define	spec_mkdir	genfs_badop
#define	spec_rmdir	genfs_badop
#define	spec_symlink	genfs_badop
#define	spec_readdir	genfs_badop
#define	spec_readlink	genfs_badop
#define	spec_abortop	genfs_badop
int	spec_inactive(void *);
int	spec_reclaim(void *);
#define	spec_lock	genfs_nolock
#define	spec_unlock	genfs_nounlock
int	spec_bmap(void *);
int	spec_strategy(void *);
int	spec_print(void *);
#define	spec_islocked	genfs_noislocked
int	spec_pathconf(void *);
int	spec_advlock(void *);
#define	spec_bwrite	vn_bwrite
#define	spec_getpages	genfs_getpages
#define	spec_putpages	genfs_putpages

bool	iskmemvp(struct vnode *);
void	spec_init(void);

#endif /* _MISCFS_SPECFS_SPECDEV_H_ */
