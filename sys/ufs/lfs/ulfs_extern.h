/*	$NetBSD: ulfs_extern.h,v 1.20 2015/09/21 01:24:23 dholland Exp $	*/
/*  from NetBSD: ufs_extern.h,v 1.72 2012/05/09 00:21:18 riastradh Exp  */

/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)ufs_extern.h	8.10 (Berkeley) 5/14/95
 */

#ifndef _UFS_LFS_ULFS_EXTERN_H_
#define _UFS_LFS_ULFS_EXTERN_H_

#include <sys/mutex.h>

struct buf;
struct componentname;
struct disklabel;
struct dquot;
struct fid;
struct flock;
struct indir;
struct inode;
struct mbuf;
struct mount;
struct nameidata;
struct lfs_direct;
struct lwp;
struct ulfs_args;
struct ulfs_lookup_results;
struct ulfs_ufid;
struct ulfsmount;
struct uio;
struct vattr;
struct vnode;

__BEGIN_DECLS
#define	ulfs_abortop	genfs_abortop
int	ulfs_access(void *);
int	ulfs_advlock(void *);
int	ulfs_bmap(void *);
int	ulfs_close(void *);
int	ulfs_create(void *);
int	ulfs_getattr(void *);
int	ulfs_inactive(void *);
#define	ulfs_fcntl	genfs_fcntl
#define	ulfs_ioctl	genfs_enoioctl
#define	ulfs_islocked	genfs_islocked
int	ulfs_link(void *);
#define	ulfs_lock	genfs_lock
int	ulfs_lookup(void *);
#define	ulfs_mmap	genfs_mmap
#define	ulfs_revoke	genfs_revoke
int	ulfs_open(void *);
int	ulfs_pathconf(void *);
int	ulfs_print(void *);
int	ulfs_readdir(void *);
int	ulfs_readlink(void *);
int	ulfs_remove(void *);
int	ulfs_rmdir(void *);
#define	ulfs_seek	genfs_seek
#define	ulfs_poll	genfs_poll
int	ulfs_setattr(void *);
int	ulfs_strategy(void *);
#define	ulfs_unlock	genfs_unlock
int	ulfs_whiteout(void *);
int	ulfsspec_close(void *);
int	ulfsspec_read(void *);
int	ulfsspec_write(void *);

int	ulfsfifo_read(void *);
int	ulfsfifo_write(void *);
int	ulfsfifo_close(void *);

/* ulfs_bmap.c */
typedef	bool (*ulfs_issequential_callback_t)(const struct lfs *,
						 daddr_t, daddr_t);
int	ulfs_bmaparray(struct vnode *, daddr_t, daddr_t *, struct indir *,
		      int *, int *, ulfs_issequential_callback_t);
int	ulfs_getlbns(struct vnode *, daddr_t, struct indir *, int *);

/* ulfs_inode.c */
int	ulfs_reclaim(struct vnode *);
int	ulfs_balloc_range(struct vnode *, off_t, off_t, kauth_cred_t, int);

/* ulfs_lookup.c */
void	ulfs_dirbad(struct inode *, doff_t, const char *);
int	ulfs_dirbadentry(struct vnode *, LFS_DIRHEADER *, int);
int	ulfs_direnter(struct vnode *, const struct ulfs_lookup_results *,
		     struct vnode *,
		     struct componentname *, ino_t, unsigned,
		     struct buf *);
int	ulfs_dirremove(struct vnode *, const struct ulfs_lookup_results *,
		      struct inode *, int, int);
int	ulfs_dirrewrite(struct inode *, off_t,
		       struct inode *, ino_t, int, int, int);
int	ulfs_dirempty(struct inode *, ino_t, kauth_cred_t);
int	ulfs_blkatoff(struct vnode *, off_t, char **, struct buf **, bool);

/* ulfs_quota.c */
/*
 * Flags to lfs_chkdq() and lfs_chkiq()
 */
#define	FORCE	0x01	/* force usage changes independent of limits */
void	ulfsquota_init(struct inode *);
void	ulfsquota_free(struct inode *);
int	lfs_chkdq(struct inode *, int64_t, kauth_cred_t, int);
int	lfs_chkiq(struct inode *, int32_t, kauth_cred_t, int);
int	lfsquota_handle_cmd(struct mount *, struct lwp *,
			 struct quotactl_args *);

int	lfs_qsync(struct mount *);

/* ulfs_quota1.c */
int	lfsquota1_umount(struct mount *, int);

/* ulfs_quota2.c */
int	lfsquota2_umount(struct mount *, int);
int	lfs_quota2_mount(struct mount *);

/* ulfs_vfsops.c */
void	ulfs_init(void);
void	ulfs_reinit(void);
void	ulfs_done(void);
int	ulfs_start(struct mount *, int);
int	ulfs_root(struct mount *, struct vnode **);
int	ulfs_quotactl(struct mount *, struct quotactl_args *);
int	ulfs_fhtovp(struct mount *, struct ulfs_ufid *, struct vnode **);

/* ulfs_vnops.c */
void	ulfs_vinit(struct mount *, int (**)(void *),
		  int (**)(void *), struct vnode **);
int	ulfs_makeinode(struct vattr *vap, struct vnode *,
		      const struct ulfs_lookup_results *,
		      struct vnode **, struct componentname *);
int	ulfs_gop_alloc(struct vnode *, off_t, off_t, int, kauth_cred_t);
void	ulfs_gop_markupdate(struct vnode *, int);
int	ulfs_bufio(enum uio_rw, struct vnode *, void *, size_t, off_t, int,
	    kauth_cred_t, size_t *, struct lwp *);

/*
 * Snapshot function prototypes.
 */

void	ulfs_snapgone(struct inode *);

__END_DECLS

#endif /* !_UFS_LFS_ULFS_EXTERN_H_ */
