/*	$NetBSD: layer_extern.h,v 1.36 2014/05/25 13:51:25 hannken Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 */

#ifndef _MISCFS_GENFS_LAYER_EXTERN_H_
#define _MISCFS_GENFS_LAYER_EXTERN_H_

/*
 * Routines defined by layerfs
 */

/* Routines to manage nodes. */
void	layerfs_init(void);
void	layerfs_done(void);
int	layer_node_create(struct mount *, struct vnode *, struct vnode **);

/* VFS routines */
int	layerfs_start(struct mount *, int);
int	layerfs_root(struct mount *, struct vnode **);
int	layerfs_quotactl(struct mount *, struct quotactl_args *);
int	layerfs_statvfs(struct mount *, struct statvfs *);
int	layerfs_sync(struct mount *, int, struct kauth_cred *);
int	layerfs_loadvnode(struct mount *,  struct vnode *,
	    const void *, size_t, const void **);
int	layerfs_vget(struct mount *, ino_t, struct vnode **);
int	layerfs_fhtovp(struct mount *, struct fid *, struct vnode **);
int	layerfs_vptofh(struct vnode *, struct fid *, size_t *);
int	layerfs_snapshot(struct mount *, struct vnode *, struct timespec *);
int	layerfs_renamelock_enter(struct mount *);
void	layerfs_renamelock_exit(struct mount *);

/* VOP routines */
int	layer_bypass(void *);
int	layer_getattr(void *);
int	layer_inactive(void *);
int	layer_reclaim(void *);
int	layer_lock(void *);
int	layer_print(void *);
int	layer_bmap(void *);
int	layer_fsync(void *);
int	layer_lookup(void *);
int	layer_setattr(void *);
int	layer_access(void *);
int	layer_open(void *);
int	layer_remove(void *);
int	layer_rename(void *);
int	layer_revoke(void *);
int	layer_rmdir(void *);
int	layer_getpages(void *);
int	layer_putpages(void *);

#endif /* _MISCFS_GENFS_LAYER_EXTERN_H_ */
