/*	$NetBSD: ufs_extern.h,v 1.79 2015/03/27 17:27:56 riastradh Exp $	*/

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

#ifndef _UFS_UFS_EXTERN_H_
#define _UFS_UFS_EXTERN_H_

#include <sys/mutex.h>

struct buf;
struct componentname;
struct direct;
struct disklabel;
struct dquot;
struct fid;
struct flock;
struct indir;
struct inode;
struct mbuf;
struct mount;
struct nameidata;
struct lwp;
struct ufid;
struct ufs_args;
struct ufs_lookup_results;
struct ufsmount;
struct uio;
struct vattr;
struct vnode;

extern pool_cache_t ufs_direct_cache;	/* memory pool for directs */

__BEGIN_DECLS
#define	ufs_abortop	genfs_abortop
int	ufs_access(void *);
int	ufs_advlock(void *);
int	ufs_bmap(void *);
int	ufs_close(void *);
int	ufs_create(void *);
int	ufs_getattr(void *);
int	ufs_inactive(void *);
#define	ufs_fcntl	genfs_fcntl
#define	ufs_ioctl	genfs_enoioctl
#define	ufs_islocked	genfs_islocked
int	ufs_link(void *);
#define	ufs_lock	genfs_lock
int	ufs_lookup(void *);
int	ufs_mkdir(void *);
int	ufs_mknod(void *);
#define	ufs_mmap	genfs_mmap
#define	ufs_revoke	genfs_revoke
int	ufs_open(void *);
int	ufs_pathconf(void *);
int	ufs_print(void *);
int	ufs_readdir(void *);
int	ufs_readlink(void *);
int	ufs_remove(void *);
int	ufs_rename(void *);
int	ufs_rmdir(void *);
#define	ufs_seek	genfs_seek
#define	ufs_poll	genfs_poll
int	ufs_setattr(void *);
int	ufs_strategy(void *);
int	ufs_symlink(void *);
#define	ufs_unlock	genfs_unlock
int	ufs_whiteout(void *);
int	ufsspec_close(void *);
int	ufsspec_read(void *);
int	ufsspec_write(void *);

int	ufsfifo_read(void *);
int	ufsfifo_write(void *);
int	ufsfifo_close(void *);

/* ufs_bmap.c */
typedef	bool (*ufs_issequential_callback_t)(const struct ufsmount *,
						 daddr_t, daddr_t);
int	ufs_bmaparray(struct vnode *, daddr_t, daddr_t *, struct indir *,
		      int *, int *, ufs_issequential_callback_t);
int	ufs_getlbns(struct vnode *, daddr_t, struct indir *, int *);

/* ufs_inode.c */
int	ufs_reclaim(struct vnode *);
int	ufs_balloc_range(struct vnode *, off_t, off_t, kauth_cred_t, int);
int	ufs_truncate(struct vnode *, uint64_t, kauth_cred_t);

/* ufs_lookup.c */
void	ufs_dirbad(struct inode *, doff_t, const char *);
int	ufs_dirbadentry(struct vnode *, struct direct *, int);
void	ufs_makedirentry(struct inode *, struct componentname *,
			 struct direct *);
int	ufs_direnter(struct vnode *, const struct ufs_lookup_results *,
		     struct vnode *, struct direct *,
		     struct componentname *, struct buf *);
int	ufs_dirremove(struct vnode *, const struct ufs_lookup_results *,
		      struct inode *, int, int);
int	ufs_dirrewrite(struct inode *, off_t,
		       struct inode *, ino_t, int, int, int);
int	ufs_dirempty(struct inode *, ino_t, kauth_cred_t);
int	ufs_blkatoff(struct vnode *, off_t, char **, struct buf **, bool);

/* ufs_rename.c -- for lfs */
bool	ufs_gro_directory_empty_p(struct mount *, kauth_cred_t,
	    struct vnode *, struct vnode *);
int	ufs_gro_rename_check_possible(struct mount *,
	    struct vnode *, struct vnode *, struct vnode *, struct vnode *);
int	ufs_gro_rename_check_permitted(struct mount *, kauth_cred_t,
	    struct vnode *, struct vnode *, struct vnode *, struct vnode *);
int	ufs_gro_remove_check_possible(struct mount *,
	    struct vnode *, struct vnode *);
int	ufs_gro_remove_check_permitted(struct mount *, kauth_cred_t,
	    struct vnode *, struct vnode *);
int	ufs_gro_rename(struct mount *, kauth_cred_t,
	    struct vnode *, struct componentname *, void *, struct vnode *,
	    struct vnode *, struct componentname *, void *, struct vnode *);
int	ufs_gro_remove(struct mount *, kauth_cred_t,
	    struct vnode *, struct componentname *, void *, struct vnode *);
int	ufs_gro_lookup(struct mount *, struct vnode *,
	    struct componentname *, void *, struct vnode **);
int	ufs_gro_genealogy(struct mount *, kauth_cred_t,
	    struct vnode *, struct vnode *, struct vnode **);
int	ufs_gro_lock_directory(struct mount *, struct vnode *);


/* ufs_quota.c */
/*
 * Flags to chkdq() and chkiq()
 */
#define	FORCE	0x01	/* force usage changes independent of limits */
void	ufsquota_init(struct inode *);
void	ufsquota_free(struct inode *);
int	chkdq(struct inode *, int64_t, kauth_cred_t, int);
int	chkiq(struct inode *, int32_t, kauth_cred_t, int);
int	quota_handle_cmd(struct mount *, struct lwp *,
			 struct quotactl_args *);

int	qsync(struct mount *);

/* ufs_quota1.c */
int	quota1_umount(struct mount *, int);

/* ufs_quota2.c */
int	quota2_umount(struct mount *, int);

/* ufs_vfsops.c */
void	ufs_init(void);
void	ufs_reinit(void);
void	ufs_done(void);
int	ufs_start(struct mount *, int);
int	ufs_root(struct mount *, struct vnode **);
int	ufs_vget(struct mount *, ino_t, struct vnode **);
int	ufs_quotactl(struct mount *, struct quotactl_args *);
int	ufs_fhtovp(struct mount *, struct ufid *, struct vnode **);

/* ufs_vnops.c */
void	ufs_vinit(struct mount *, int (**)(void *),
		  int (**)(void *), struct vnode **);
int	ufs_gop_alloc(struct vnode *, off_t, off_t, int, kauth_cred_t);
void	ufs_gop_markupdate(struct vnode *, int);
int	ufs_bufio(enum uio_rw, struct vnode *, void *, size_t, off_t, int,
	    kauth_cred_t, size_t *, struct lwp *);

__END_DECLS

extern kmutex_t ufs_hashlock;

#endif /* !_UFS_UFS_EXTERN_H_ */
