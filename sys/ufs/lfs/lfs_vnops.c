/*	$NetBSD: lfs_vnops.c,v 1.238 2011/09/20 14:01:33 chs Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 * Copyright (c) 1986, 1989, 1991, 1993, 1995
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
 *	@(#)lfs_vnops.c	8.13 (Berkeley) 6/10/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_vnops.c,v 1.238 2011/09/20 14:01:33 chs Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#include "opt_uvm_page_trkown.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/pool.h>
#include <sys/signalvar.h>
#include <sys/kauth.h>
#include <sys/syslog.h>
#include <sys/fstrans.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pmap.h>
#include <uvm/uvm_stat.h>
#include <uvm/uvm_pager.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

extern pid_t lfs_writer_daemon;
int lfs_ignore_lazy_sync = 1;

/* Global vfs data structures for lfs. */
int (**lfs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc lfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, ufs_lookup },		/* lookup */
	{ &vop_create_desc, lfs_create },		/* create */
	{ &vop_whiteout_desc, ufs_whiteout },		/* whiteout */
	{ &vop_mknod_desc, lfs_mknod },			/* mknod */
	{ &vop_open_desc, ufs_open },			/* open */
	{ &vop_close_desc, lfs_close },			/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, lfs_getattr },		/* getattr */
	{ &vop_setattr_desc, lfs_setattr },		/* setattr */
	{ &vop_read_desc, lfs_read },			/* read */
	{ &vop_write_desc, lfs_write },			/* write */
	{ &vop_ioctl_desc, ufs_ioctl },			/* ioctl */
	{ &vop_fcntl_desc, lfs_fcntl },			/* fcntl */
	{ &vop_poll_desc, ufs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, ufs_revoke },		/* revoke */
	{ &vop_mmap_desc, lfs_mmap },			/* mmap */
	{ &vop_fsync_desc, lfs_fsync },			/* fsync */
	{ &vop_seek_desc, ufs_seek },			/* seek */
	{ &vop_remove_desc, lfs_remove },		/* remove */
	{ &vop_link_desc, lfs_link },			/* link */
	{ &vop_rename_desc, lfs_rename },		/* rename */
	{ &vop_mkdir_desc, lfs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, lfs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, lfs_symlink },		/* symlink */
	{ &vop_readdir_desc, ufs_readdir },		/* readdir */
	{ &vop_readlink_desc, ufs_readlink },		/* readlink */
	{ &vop_abortop_desc, ufs_abortop },		/* abortop */
	{ &vop_inactive_desc, lfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, lfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, ufs_bmap },			/* bmap */
	{ &vop_strategy_desc, lfs_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ufs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ufs_advlock },		/* advlock */
	{ &vop_bwrite_desc, lfs_bwrite },		/* bwrite */
	{ &vop_getpages_desc, lfs_getpages },		/* getpages */
	{ &vop_putpages_desc, lfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc lfs_vnodeop_opv_desc =
	{ &lfs_vnodeop_p, lfs_vnodeop_entries };

int (**lfs_specop_p)(void *);
const struct vnodeopv_entry_desc lfs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, lfsspec_close },		/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, lfs_getattr },		/* getattr */
	{ &vop_setattr_desc, lfs_setattr },		/* setattr */
	{ &vop_read_desc, ufsspec_read },		/* read */
	{ &vop_write_desc, ufsspec_write },		/* write */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_seek_desc, spec_seek },			/* seek */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, lfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, lfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc lfs_specop_opv_desc =
	{ &lfs_specop_p, lfs_specop_entries };

int (**lfs_fifoop_p)(void *);
const struct vnodeopv_entry_desc lfs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup */
	{ &vop_create_desc, vn_fifo_bypass },		/* create */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* mknod */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, lfsfifo_close },		/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, lfs_getattr },		/* getattr */
	{ &vop_setattr_desc, lfs_setattr },		/* setattr */
	{ &vop_read_desc, ufsfifo_read },		/* read */
	{ &vop_write_desc, ufsfifo_write },		/* write */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_fcntl_desc, ufs_fcntl },			/* fcntl */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* mmap */
	{ &vop_fsync_desc, vn_fifo_bypass },		/* fsync */
	{ &vop_seek_desc, vn_fifo_bypass },		/* seek */
	{ &vop_remove_desc, vn_fifo_bypass },		/* remove */
	{ &vop_link_desc, vn_fifo_bypass },		/* link */
	{ &vop_rename_desc, vn_fifo_bypass },		/* rename */
	{ &vop_mkdir_desc, vn_fifo_bypass },		/* mkdir */
	{ &vop_rmdir_desc, vn_fifo_bypass },		/* rmdir */
	{ &vop_symlink_desc, vn_fifo_bypass },		/* symlink */
	{ &vop_readdir_desc, vn_fifo_bypass },		/* readdir */
	{ &vop_readlink_desc, vn_fifo_bypass },		/* readlink */
	{ &vop_abortop_desc, vn_fifo_bypass },		/* abortop */
	{ &vop_inactive_desc, lfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, lfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, lfs_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc lfs_fifoop_opv_desc =
	{ &lfs_fifoop_p, lfs_fifoop_entries };

static int check_dirty(struct lfs *, struct vnode *, off_t, off_t, off_t, int, int, struct vm_page **);

#define	LFS_READWRITE
#include <ufs/ufs/ufs_readwrite.c>
#undef	LFS_READWRITE

/*
 * Synch an open file.
 */
/* ARGSUSED */
int
lfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		int a_flags;
		off_t offlo;
		off_t offhi;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error, wait;
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;

	/* If we're mounted read-only, don't try to sync. */
	if (fs->lfs_ronly)
		return 0;

	/* If a removed vnode is being cleaned, no need to sync here. */
	if ((ap->a_flags & FSYNC_RECLAIM) != 0 && ip->i_mode == 0)
		return 0;

	/*
	 * Trickle sync simply adds this vnode to the pager list, as if
	 * the pagedaemon had requested a pageout.
	 */
	if (ap->a_flags & FSYNC_LAZY) {
		if (lfs_ignore_lazy_sync == 0) {
			mutex_enter(&lfs_lock);
			if (!(ip->i_flags & IN_PAGING)) {
				ip->i_flags |= IN_PAGING;
				TAILQ_INSERT_TAIL(&fs->lfs_pchainhd, ip,
						  i_lfs_pchain);
			}
			wakeup(&lfs_writer_daemon);
			mutex_exit(&lfs_lock);
		}
		return 0;
	}

	/*
	 * If a vnode is bring cleaned, flush it out before we try to
	 * reuse it.  This prevents the cleaner from writing files twice
	 * in the same partial segment, causing an accounting underflow.
	 */
	if (ap->a_flags & FSYNC_RECLAIM && ip->i_flags & IN_CLEANING) {
		lfs_vflush(vp);
	}

	wait = (ap->a_flags & FSYNC_WAIT);
	do {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp, trunc_page(ap->a_offlo),
				     round_page(ap->a_offhi),
				     PGO_CLEANIT | (wait ? PGO_SYNCIO : 0));
		if (error == EAGAIN) {
			mutex_enter(&lfs_lock);
			mtsleep(&fs->lfs_avail, PCATCH | PUSER, "lfs_fsync",
				hz / 100 + 1, &lfs_lock);
			mutex_exit(&lfs_lock);
		}
	} while (error == EAGAIN);
	if (error)
		return error;

	if ((ap->a_flags & FSYNC_DATAONLY) == 0)
		error = lfs_update(vp, NULL, NULL, wait ? UPDATE_WAIT : 0);

	if (error == 0 && ap->a_flags & FSYNC_CACHE) {
		int l = 0;
		error = VOP_IOCTL(ip->i_devvp, DIOCCACHESYNC, &l, FWRITE,
				  curlwp->l_cred);
	}
	if (wait && !VPISEMPTY(vp))
		LFS_SET_UINO(ip, IN_MODIFIED);

	return error;
}

/*
 * Take IN_ADIROP off, then call ufs_inactive.
 */
int
lfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	lfs_unmark_vnode(ap->a_vp);

	/*
	 * The Ifile is only ever inactivated on unmount.
	 * Streamline this process by not giving it more dirty blocks.
	 */
	if (VTOI(ap->a_vp)->i_number == LFS_IFILE_INUM) {
		mutex_enter(&lfs_lock);
		LFS_CLR_UINO(VTOI(ap->a_vp), IN_ALLMOD);
		mutex_exit(&lfs_lock);
		VOP_UNLOCK(ap->a_vp);
		return 0;
	}

	return ufs_inactive(v);
}

/*
 * These macros are used to bracket UFS directory ops, so that we can
 * identify all the pages touched during directory ops which need to
 * be ordered and flushed atomically, so that they may be recovered.
 *
 * Because we have to mark nodes VU_DIROP in order to prevent
 * the cache from reclaiming them while a dirop is in progress, we must
 * also manage the number of nodes so marked (otherwise we can run out).
 * We do this by setting lfs_dirvcount to the number of marked vnodes; it
 * is decremented during segment write, when VU_DIROP is taken off.
 */
#define	MARK_VNODE(vp)			lfs_mark_vnode(vp)
#define	UNMARK_VNODE(vp)		lfs_unmark_vnode(vp)
#define	SET_DIROP_CREATE(dvp, vpp)	lfs_set_dirop_create((dvp), (vpp))
#define	SET_DIROP_REMOVE(dvp, vp)	lfs_set_dirop((dvp), (vp))
static int lfs_set_dirop_create(struct vnode *, struct vnode **);
static int lfs_set_dirop(struct vnode *, struct vnode *);

static int
lfs_set_dirop(struct vnode *dvp, struct vnode *vp)
{
	struct lfs *fs;
	int error;

	KASSERT(VOP_ISLOCKED(dvp));
	KASSERT(vp == NULL || VOP_ISLOCKED(vp));

	fs = VTOI(dvp)->i_lfs;

	ASSERT_NO_SEGLOCK(fs);
	/*
	 * LFS_NRESERVE calculates direct and indirect blocks as well
	 * as an inode block; an overestimate in most cases.
	 */
	if ((error = lfs_reserve(fs, dvp, vp, LFS_NRESERVE(fs))) != 0)
		return (error);

    restart:
	mutex_enter(&lfs_lock);
	if (fs->lfs_dirops == 0) {
		mutex_exit(&lfs_lock);
		lfs_check(dvp, LFS_UNUSED_LBN, 0);
		mutex_enter(&lfs_lock);
	}
	while (fs->lfs_writer) {
		error = mtsleep(&fs->lfs_dirops, (PRIBIO + 1) | PCATCH,
		    "lfs_sdirop", 0, &lfs_lock);
		if (error == EINTR) {
			mutex_exit(&lfs_lock);
			goto unreserve;
		}
	}
	if (lfs_dirvcount > LFS_MAX_DIROP && fs->lfs_dirops == 0) {
		wakeup(&lfs_writer_daemon);
		mutex_exit(&lfs_lock);
		preempt();
		goto restart;
	}

	if (lfs_dirvcount > LFS_MAX_DIROP) {
		mutex_exit(&lfs_lock);
		DLOG((DLOG_DIROP, "lfs_set_dirop: sleeping with dirops=%d, "
		      "dirvcount=%d\n", fs->lfs_dirops, lfs_dirvcount));
		if ((error = mtsleep(&lfs_dirvcount,
		    PCATCH | PUSER | PNORELOCK, "lfs_maxdirop", 0,
		    &lfs_lock)) != 0) {
			goto unreserve;
		}
		goto restart;
	}

	++fs->lfs_dirops;
	fs->lfs_doifile = 1;
	mutex_exit(&lfs_lock);

	/* Hold a reference so SET_ENDOP will be happy */
	vref(dvp);
	if (vp) {
		vref(vp);
		MARK_VNODE(vp);
	}

	MARK_VNODE(dvp);
	return 0;

  unreserve:
	lfs_reserve(fs, dvp, vp, -LFS_NRESERVE(fs));
	return error;
}

/*
 * Get a new vnode *before* adjusting the dirop count, to avoid a deadlock
 * in getnewvnode(), if we have a stacked filesystem mounted on top
 * of us.
 *
 * NB: this means we have to clear the new vnodes on error.  Fortunately
 * SET_ENDOP is there to do that for us.
 */
static int
lfs_set_dirop_create(struct vnode *dvp, struct vnode **vpp)
{
	int error;
	struct lfs *fs;

	fs = VFSTOUFS(dvp->v_mount)->um_lfs;
	ASSERT_NO_SEGLOCK(fs);
	if (fs->lfs_ronly)
		return EROFS;
	if (vpp == NULL) {
		return lfs_set_dirop(dvp, NULL);
	}
	error = getnewvnode(VT_LFS, dvp->v_mount, lfs_vnodeop_p, NULL, vpp);
	if (error) {
		DLOG((DLOG_ALLOC, "lfs_set_dirop_create: dvp %p error %d\n",
		      dvp, error));
		return error;
	}
	if ((error = lfs_set_dirop(dvp, NULL)) != 0) {
		ungetnewvnode(*vpp);
		*vpp = NULL;
		return error;
	}
	return 0;
}

#define	SET_ENDOP_BASE(fs, dvp, str)					\
	do {								\
		mutex_enter(&lfs_lock);				\
		--(fs)->lfs_dirops;					\
		if (!(fs)->lfs_dirops) {				\
			if ((fs)->lfs_nadirop) {			\
				panic("SET_ENDOP: %s: no dirops but "	\
					" nadirop=%d", (str),		\
					(fs)->lfs_nadirop);		\
			}						\
			wakeup(&(fs)->lfs_writer);			\
			mutex_exit(&lfs_lock);				\
			lfs_check((dvp), LFS_UNUSED_LBN, 0);		\
		} else							\
			mutex_exit(&lfs_lock);				\
	} while(0)
#define SET_ENDOP_CREATE(fs, dvp, nvpp, str)				\
	do {								\
		UNMARK_VNODE(dvp);					\
		if (nvpp && *nvpp)					\
			UNMARK_VNODE(*nvpp);				\
		/* Check for error return to stem vnode leakage */	\
		if (nvpp && *nvpp && !((*nvpp)->v_uflag & VU_DIROP))	\
			ungetnewvnode(*(nvpp));				\
		SET_ENDOP_BASE((fs), (dvp), (str));			\
		lfs_reserve((fs), (dvp), NULL, -LFS_NRESERVE(fs));	\
		vrele(dvp);						\
	} while(0)
#define SET_ENDOP_CREATE_AP(ap, str)					\
	SET_ENDOP_CREATE(VTOI((ap)->a_dvp)->i_lfs, (ap)->a_dvp,		\
			 (ap)->a_vpp, (str))
#define SET_ENDOP_REMOVE(fs, dvp, ovp, str)				\
	do {								\
		UNMARK_VNODE(dvp);					\
		if (ovp)						\
			UNMARK_VNODE(ovp);				\
		SET_ENDOP_BASE((fs), (dvp), (str));			\
		lfs_reserve((fs), (dvp), (ovp), -LFS_NRESERVE(fs));	\
		vrele(dvp);						\
		if (ovp)						\
			vrele(ovp);					\
	} while(0)

void
lfs_mark_vnode(struct vnode *vp)
{
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;

	mutex_enter(&lfs_lock);
	if (!(ip->i_flag & IN_ADIROP)) {
		if (!(vp->v_uflag & VU_DIROP)) {
			mutex_enter(vp->v_interlock);
			(void)lfs_vref(vp);
			++lfs_dirvcount;
			++fs->lfs_dirvcount;
			TAILQ_INSERT_TAIL(&fs->lfs_dchainhd, ip, i_lfs_dchain);
			vp->v_uflag |= VU_DIROP;
		}
		++fs->lfs_nadirop;
		ip->i_flag |= IN_ADIROP;
	} else
		KASSERT(vp->v_uflag & VU_DIROP);
	mutex_exit(&lfs_lock);
}

void
lfs_unmark_vnode(struct vnode *vp)
{
	struct inode *ip = VTOI(vp);

	if (ip && (ip->i_flag & IN_ADIROP)) {
		KASSERT(vp->v_uflag & VU_DIROP);
		mutex_enter(&lfs_lock);
		--ip->i_lfs->lfs_nadirop;
		mutex_exit(&lfs_lock);
		ip->i_flag &= ~IN_ADIROP;
	}
}

int
lfs_symlink(void *v)
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	int error;

	if ((error = SET_DIROP_CREATE(ap->a_dvp, ap->a_vpp)) != 0) {
		vput(ap->a_dvp);
		return error;
	}
	error = ufs_symlink(ap);
	SET_ENDOP_CREATE_AP(ap, "symlink");
	return (error);
}

int
lfs_mknod(void *v)
{
	struct vop_mknod_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp = ap->a_vpp;
	struct inode *ip;
	int error;
	struct mount	*mp;
	ino_t		ino;
	struct ufs_lookup_results *ulr;

	/* XXX should handle this material another way */
	ulr = &VTOI(ap->a_dvp)->i_crap;
	UFS_CHECK_CRAPCOUNTER(VTOI(ap->a_dvp));

	if ((error = SET_DIROP_CREATE(ap->a_dvp, ap->a_vpp)) != 0) {
		vput(ap->a_dvp);
		return error;
	}
	error = ufs_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
			      ap->a_dvp, ulr, vpp, ap->a_cnp);

	/* Either way we're done with the dirop at this point */
	SET_ENDOP_CREATE_AP(ap, "mknod");

	if (error)
		return (error);

	ip = VTOI(*vpp);
	mp  = (*vpp)->v_mount;
	ino = ip->i_number;
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
#if 0
		ip->i_ffs1_rdev = ufs_rw32(vap->va_rdev,
					   UFS_MPNEEDSWAP((*vpp)->v_mount));
#else
		ip->i_ffs1_rdev = vap->va_rdev;
#endif
	}

	/*
	 * Call fsync to write the vnode so that we don't have to deal with
	 * flushing it when it's marked VU_DIROP|VI_XLOCK.
	 *
	 * XXX KS - If we can't flush we also can't call vgone(), so must
	 * return.  But, that leaves this vnode in limbo, also not good.
	 * Can this ever happen (barring hardware failure)?
	 */
	if ((error = VOP_FSYNC(*vpp, NOCRED, FSYNC_WAIT, 0, 0)) != 0) {
		panic("lfs_mknod: couldn't fsync (ino %llu)",
		      (unsigned long long)ino);
		/* return (error); */
	}
	/*
	 * Remove vnode so that it will be reloaded by VFS_VGET and
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	/* Used to be vput, but that causes us to call VOP_INACTIVE twice. */

	VOP_UNLOCK(*vpp);
	(*vpp)->v_type = VNON;
	vgone(*vpp);
	error = VFS_VGET(mp, ino, vpp);

	if (error != 0) {
		*vpp = NULL;
		return (error);
	}
	return (0);
}

int
lfs_create(void *v)
{
	struct vop_create_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	int error;

	if ((error = SET_DIROP_CREATE(ap->a_dvp, ap->a_vpp)) != 0) {
		vput(ap->a_dvp);
		return error;
	}
	error = ufs_create(ap);
	SET_ENDOP_CREATE_AP(ap, "create");
	return (error);
}

int
lfs_mkdir(void *v)
{
	struct vop_mkdir_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	int error;

	if ((error = SET_DIROP_CREATE(ap->a_dvp, ap->a_vpp)) != 0) {
		vput(ap->a_dvp);
		return error;
	}
	error = ufs_mkdir(ap);
	SET_ENDOP_CREATE_AP(ap, "mkdir");
	return (error);
}

int
lfs_remove(void *v)
{
	struct vop_remove_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *dvp, *vp;
	struct inode *ip;
	int error;

	dvp = ap->a_dvp;
	vp = ap->a_vp;
	ip = VTOI(vp);
	if ((error = SET_DIROP_REMOVE(dvp, vp)) != 0) {
		if (dvp == vp)
			vrele(vp);
		else
			vput(vp);
		vput(dvp);
		return error;
	}
	error = ufs_remove(ap);
	if (ip->i_nlink == 0)
		lfs_orphan(ip->i_lfs, ip->i_number);
	SET_ENDOP_REMOVE(ip->i_lfs, dvp, ap->a_vp, "remove");
	return (error);
}

int
lfs_rmdir(void *v)
{
	struct vop_rmdir_args	/* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp;
	struct inode *ip;
	int error;

	vp = ap->a_vp;
	ip = VTOI(vp);
	if ((error = SET_DIROP_REMOVE(ap->a_dvp, ap->a_vp)) != 0) {
		if (ap->a_dvp == vp)
			vrele(ap->a_dvp);
		else
			vput(ap->a_dvp);
		vput(vp);
		return error;
	}
	error = ufs_rmdir(ap);
	if (ip->i_nlink == 0)
		lfs_orphan(ip->i_lfs, ip->i_number);
	SET_ENDOP_REMOVE(ip->i_lfs, ap->a_dvp, ap->a_vp, "rmdir");
	return (error);
}

int
lfs_link(void *v)
{
	struct vop_link_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	int error;
	struct vnode **vpp = NULL;

	if ((error = SET_DIROP_CREATE(ap->a_dvp, vpp)) != 0) {
		vput(ap->a_dvp);
		return error;
	}
	error = ufs_link(ap);
	SET_ENDOP_CREATE(VTOI(ap->a_dvp)->i_lfs, ap->a_dvp, vpp, "link");
	return (error);
}

int
lfs_rename(void *v)
{
	struct vop_rename_args	/* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *tvp, *fvp, *tdvp, *fdvp;
	struct componentname *tcnp, *fcnp;
	int error;
	struct lfs *fs;

	fs = VTOI(ap->a_fdvp)->i_lfs;
	tvp = ap->a_tvp;
	tdvp = ap->a_tdvp;
	tcnp = ap->a_tcnp;
	fvp = ap->a_fvp;
	fdvp = ap->a_fdvp;
	fcnp = ap->a_fcnp;

	/*
	 * Check for cross-device rename.
	 * If it is, we don't want to set dirops, just error out.
	 * (In particular note that MARK_VNODE(tdvp) will DTWT on
	 * a cross-device rename.)
	 *
	 * Copied from ufs_rename.
	 */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto errout;
	}

	/*
	 * Check to make sure we're not renaming a vnode onto itself
	 * (deleting a hard link by renaming one name onto another);
	 * if we are we can't recursively call VOP_REMOVE since that
	 * would leave us with an unaccounted-for number of live dirops.
	 *
	 * Inline the relevant section of ufs_rename here, *before*
	 * calling SET_DIROP_REMOVE.
	 */
	if (tvp && ((VTOI(tvp)->i_flags & (IMMUTABLE | APPEND)) ||
		    (VTOI(tdvp)->i_flags & APPEND))) {
		error = EPERM;
		goto errout;
	}
	if (fvp == tvp) {
		if (fvp->v_type == VDIR) {
			error = EINVAL;
			goto errout;
		}

		/* Release destination completely. */
		VOP_ABORTOP(tdvp, tcnp);
		vput(tdvp);
		vput(tvp);

		/* Delete source. */
		vrele(fvp);
		fcnp->cn_flags &= ~(MODMASK);
		fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
		fcnp->cn_nameiop = DELETE;
		vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY);
		if ((error = relookup(fdvp, &fvp, fcnp, 0))) {
			vput(fdvp);
			return (error);
		}
		return (VOP_REMOVE(fdvp, fvp, fcnp));
	}

	if ((error = SET_DIROP_REMOVE(tdvp, tvp)) != 0)
		goto errout;
	MARK_VNODE(fdvp);
	MARK_VNODE(fvp);

	error = ufs_rename(ap);
	UNMARK_VNODE(fdvp);
	UNMARK_VNODE(fvp);
	SET_ENDOP_REMOVE(fs, tdvp, tvp, "rename");
	return (error);

  errout:
	VOP_ABORTOP(tdvp, ap->a_tcnp); /* XXX, why not in NFS? */
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	VOP_ABORTOP(fdvp, ap->a_fcnp); /* XXX, why not in NFS? */
	vrele(fdvp);
	vrele(fvp);
	return (error);
}

/* XXX hack to avoid calling ITIMES in getattr */
int
lfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct vattr *vap = ap->a_vap;
	struct lfs *fs = ip->i_lfs;
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~IFMT;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = (dev_t)ip->i_ffs1_rdev;
	vap->va_size = vp->v_size;
	vap->va_atime.tv_sec = ip->i_ffs1_atime;
	vap->va_atime.tv_nsec = ip->i_ffs1_atimensec;
	vap->va_mtime.tv_sec = ip->i_ffs1_mtime;
	vap->va_mtime.tv_nsec = ip->i_ffs1_mtimensec;
	vap->va_ctime.tv_sec = ip->i_ffs1_ctime;
	vap->va_ctime.tv_nsec = ip->i_ffs1_ctimensec;
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = fsbtob(fs, (u_quad_t)ip->i_lfs_effnblks);
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	return (0);
}

/*
 * Check to make sure the inode blocks won't choke the buffer
 * cache, then call ufs_setattr as usual.
 */
int
lfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	lfs_check(vp, LFS_UNUSED_LBN, 0);
	return ufs_setattr(v);
}

/*
 * Release the block we hold on lfs_newseg wrapping.  Called on file close,
 * or explicitly from LFCNWRAPGO.  Called with the interlock held.
 */
static int
lfs_wrapgo(struct lfs *fs, struct inode *ip, int waitfor)
{
	if (fs->lfs_stoplwp != curlwp)
		return EBUSY;

	fs->lfs_stoplwp = NULL;
	cv_signal(&fs->lfs_stopcv);

	KASSERT(fs->lfs_nowrap > 0);
	if (fs->lfs_nowrap <= 0) {
		return 0;
	}

	if (--fs->lfs_nowrap == 0) {
		log(LOG_NOTICE, "%s: re-enabled log wrap\n", fs->lfs_fsmnt);
		wakeup(&fs->lfs_wrappass);
		lfs_wakeup_cleaner(fs);
	}
	if (waitfor) {
		mtsleep(&fs->lfs_nextseg, PCATCH | PUSER, "segment",
		    0, &lfs_lock);
	}

	return 0;
}

/*
 * Close called
 */
/* ARGSUSED */
int
lfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;

	if ((ip->i_number == ROOTINO || ip->i_number == LFS_IFILE_INUM) &&
	    fs->lfs_stoplwp == curlwp) {
		mutex_enter(&lfs_lock);
		log(LOG_NOTICE, "lfs_close: releasing log wrap control\n");
		lfs_wrapgo(fs, ip, 0);
		mutex_exit(&lfs_lock);
	}

	if (vp == ip->i_lfs->lfs_ivnode &&
	    vp->v_mount->mnt_iflag & IMNT_UNMOUNT)
		return 0;

	if (vp->v_usecount > 1 && vp != ip->i_lfs->lfs_ivnode) {
		LFS_ITIMES(ip, NULL, NULL, NULL);
	}
	return (0);
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
int
lfsspec_close(void *v)
{
	struct vop_close_args /* {
		struct vnode	*a_vp;
		int		a_fflag;
		kauth_cred_t	a_cred;
	} */ *ap = v;
	struct vnode	*vp;
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
	if (vp->v_usecount > 1) {
		LFS_ITIMES(ip, NULL, NULL, NULL);
	}
	return (VOCALL (spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Close wrapper for fifo's.
 *
 * Update the times on the inode then do device close.
 */
int
lfsfifo_close(void *v)
{
	struct vop_close_args /* {
		struct vnode	*a_vp;
		int		a_fflag;
		kauth_cred_	a_cred;
	} */ *ap = v;
	struct vnode	*vp;
	struct inode	*ip;

	vp = ap->a_vp;
	ip = VTOI(vp);
	if (ap->a_vp->v_usecount > 1) {
		LFS_ITIMES(ip, NULL, NULL, NULL);
	}
	return (VOCALL (fifo_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */

int
lfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;
	int error;

	/*
	 * The inode must be freed and updated before being removed
	 * from its hash chain.  Other threads trying to gain a hold
	 * on the inode will be stalled because it is locked (VI_XLOCK).
	 */
	if (ip->i_nlink <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		lfs_vfree(vp, ip->i_number, ip->i_omode);

	mutex_enter(&lfs_lock);
	LFS_CLR_UINO(ip, IN_ALLMOD);
	mutex_exit(&lfs_lock);
	if ((error = ufs_reclaim(vp)))
		return (error);

	/*
	 * Take us off the paging and/or dirop queues if we were on them.
	 * We shouldn't be on them.
	 */
	mutex_enter(&lfs_lock);
	if (ip->i_flags & IN_PAGING) {
		log(LOG_WARNING, "%s: reclaimed vnode is IN_PAGING\n",
		    fs->lfs_fsmnt);
		ip->i_flags &= ~IN_PAGING;
		TAILQ_REMOVE(&fs->lfs_pchainhd, ip, i_lfs_pchain);
	}
	if (vp->v_uflag & VU_DIROP) {
		panic("reclaimed vnode is VU_DIROP");
		vp->v_uflag &= ~VU_DIROP;
		TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
	}
	mutex_exit(&lfs_lock);

	pool_put(&lfs_dinode_pool, ip->i_din.ffs1_din);
	lfs_deregister_all(vp);
	pool_put(&lfs_inoext_pool, ip->inode_ext.lfs);
	ip->inode_ext.lfs = NULL;
	genfs_node_destroy(vp);
	pool_put(&lfs_inode_pool, vp->v_data);
	vp->v_data = NULL;
	return (0);
}

/*
 * Read a block from a storage device.
 * In order to avoid reading blocks that are in the process of being
 * written by the cleaner---and hence are not mutexed by the normal
 * buffer cache / page cache mechanisms---check for collisions before
 * reading.
 *
 * We inline ufs_strategy to make sure that the VOP_BMAP occurs *before*
 * the active cleaner test.
 *
 * XXX This code assumes that lfs_markv makes synchronous checkpoints.
 */
int
lfs_strategy(void *v)
{
	struct vop_strategy_args /* {
		struct vnode *a_vp;
		struct buf *a_bp;
	} */ *ap = v;
	struct buf	*bp;
	struct lfs	*fs;
	struct vnode	*vp;
	struct inode	*ip;
	daddr_t		tbn;
	int		i, sn, error, slept;

	bp = ap->a_bp;
	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ip->i_lfs;

	/* lfs uses its strategy routine only for read */
	KASSERT(bp->b_flags & B_READ);

	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("lfs_strategy: spec");
	KASSERT(bp->b_bcount != 0);
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno,
				 NULL);
		if (error) {
			bp->b_error = error;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			return (error);
		}
		if ((long)bp->b_blkno == -1) /* no valid data */
			clrbuf(bp);
	}
	if ((long)bp->b_blkno < 0) { /* block is not on disk */
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return (0);
	}

	slept = 1;
	mutex_enter(&lfs_lock);
	while (slept && fs->lfs_seglock) {
		mutex_exit(&lfs_lock);
		/*
		 * Look through list of intervals.
		 * There will only be intervals to look through
		 * if the cleaner holds the seglock.
		 * Since the cleaner is synchronous, we can trust
		 * the list of intervals to be current.
		 */
		tbn = dbtofsb(fs, bp->b_blkno);
		sn = dtosn(fs, tbn);
		slept = 0;
		for (i = 0; i < fs->lfs_cleanind; i++) {
			if (sn == dtosn(fs, fs->lfs_cleanint[i]) &&
			    tbn >= fs->lfs_cleanint[i]) {
				DLOG((DLOG_CLEAN,
				      "lfs_strategy: ino %d lbn %" PRId64
				      " ind %d sn %d fsb %" PRIx32
				      " given sn %d fsb %" PRIx64 "\n",
				      ip->i_number, bp->b_lblkno, i,
				      dtosn(fs, fs->lfs_cleanint[i]),
				      fs->lfs_cleanint[i], sn, tbn));
				DLOG((DLOG_CLEAN,
				      "lfs_strategy: sleeping on ino %d lbn %"
				      PRId64 "\n", ip->i_number, bp->b_lblkno));
				mutex_enter(&lfs_lock);
				if (LFS_SEGLOCK_HELD(fs) && fs->lfs_iocount) {
					/* Cleaner can't wait for itself */
					mtsleep(&fs->lfs_iocount,
						(PRIBIO + 1) | PNORELOCK,
						"clean2", 0,
						&lfs_lock);
					slept = 1;
					break;
				} else if (fs->lfs_seglock) {
					mtsleep(&fs->lfs_seglock,
						(PRIBIO + 1) | PNORELOCK,
						"clean1", 0,
						&lfs_lock);
					slept = 1;
					break;
				}
				mutex_exit(&lfs_lock);
			}
		}
		mutex_enter(&lfs_lock);
	}
	mutex_exit(&lfs_lock);

	vp = ip->i_devvp;
	VOP_STRATEGY(vp, bp);
	return (0);
}

void
lfs_flush_dirops(struct lfs *fs)
{
	struct inode *ip, *nip;
	struct vnode *vp;
	extern int lfs_dostats;
	struct segment *sp;

	ASSERT_MAYBE_SEGLOCK(fs);
	KASSERT(fs->lfs_nadirop == 0);

	if (fs->lfs_ronly)
		return;

	mutex_enter(&lfs_lock);
	if (TAILQ_FIRST(&fs->lfs_dchainhd) == NULL) {
		mutex_exit(&lfs_lock);
		return;
	} else
		mutex_exit(&lfs_lock);

	if (lfs_dostats)
		++lfs_stats.flush_invoked;

	/*
	 * Inline lfs_segwrite/lfs_writevnodes, but just for dirops.
	 * Technically this is a checkpoint (the on-disk state is valid)
	 * even though we are leaving out all the file data.
	 */
	lfs_imtime(fs);
	lfs_seglock(fs, SEGM_CKP);
	sp = fs->lfs_sp;

	/*
	 * lfs_writevnodes, optimized to get dirops out of the way.
	 * Only write dirops, and don't flush files' pages, only
	 * blocks from the directories.
	 *
	 * We don't need to vref these files because they are
	 * dirops and so hold an extra reference until the
	 * segunlock clears them of that status.
	 *
	 * We don't need to check for IN_ADIROP because we know that
	 * no dirops are active.
	 *
	 */
	mutex_enter(&lfs_lock);
	for (ip = TAILQ_FIRST(&fs->lfs_dchainhd); ip != NULL; ip = nip) {
		nip = TAILQ_NEXT(ip, i_lfs_dchain);
		mutex_exit(&lfs_lock);
		vp = ITOV(ip);

		KASSERT((ip->i_flag & IN_ADIROP) == 0);

		/*
		 * All writes to directories come from dirops; all
		 * writes to files' direct blocks go through the page
		 * cache, which we're not touching.  Reads to files
		 * and/or directories will not be affected by writing
		 * directory blocks inodes and file inodes.  So we don't
		 * really need to lock.	 If we don't lock, though,
		 * make sure that we don't clear IN_MODIFIED
		 * unnecessarily.
		 */
		if (vp->v_iflag & VI_XLOCK) {
			mutex_enter(&lfs_lock);
			continue;
		}
		/* XXX see below
		 * waslocked = VOP_ISLOCKED(vp);
		 */
		if (vp->v_type != VREG &&
		    ((ip->i_flag & IN_ALLMOD) || !VPISEMPTY(vp))) {
			lfs_writefile(fs, sp, vp);
			if (!VPISEMPTY(vp) && !WRITEINPROG(vp) &&
			    !(ip->i_flag & IN_ALLMOD)) {
			    	mutex_enter(&lfs_lock);
				LFS_SET_UINO(ip, IN_MODIFIED);
			    	mutex_exit(&lfs_lock);
			}
		}
		KDASSERT(ip->i_number != LFS_IFILE_INUM);
		(void) lfs_writeinode(fs, sp, ip);
		mutex_enter(&lfs_lock);
		/*
		 * XXX
		 * LK_EXCLOTHER is dead -- what is intended here?
		 * if (waslocked == LK_EXCLOTHER)
		 *	LFS_SET_UINO(ip, IN_MODIFIED);
		 */
	}
	mutex_exit(&lfs_lock);
	/* We've written all the dirops there are */
	((SEGSUM *)(sp->segsum))->ss_flags &= ~(SS_CONT);
	lfs_finalize_fs_seguse(fs);
	(void) lfs_writeseg(fs, sp);
	lfs_segunlock(fs);
}

/*
 * Flush all vnodes for which the pagedaemon has requested pageouts.
 * Skip over any files that are marked VU_DIROP (since lfs_flush_dirop()
 * has just run, this would be an error).  If we have to skip a vnode
 * for any reason, just skip it; if we have to wait for the cleaner,
 * abort.  The writer daemon will call us again later.
 */
void
lfs_flush_pchain(struct lfs *fs)
{
	struct inode *ip, *nip;
	struct vnode *vp;
	extern int lfs_dostats;
	struct segment *sp;
	int error;

	ASSERT_NO_SEGLOCK(fs);

	if (fs->lfs_ronly)
		return;

	mutex_enter(&lfs_lock);
	if (TAILQ_FIRST(&fs->lfs_pchainhd) == NULL) {
		mutex_exit(&lfs_lock);
		return;
	} else
		mutex_exit(&lfs_lock);

	/* Get dirops out of the way */
	lfs_flush_dirops(fs);

	if (lfs_dostats)
		++lfs_stats.flush_invoked;

	/*
	 * Inline lfs_segwrite/lfs_writevnodes, but just for pageouts.
	 */
	lfs_imtime(fs);
	lfs_seglock(fs, 0);
	sp = fs->lfs_sp;

	/*
	 * lfs_writevnodes, optimized to clear pageout requests.
	 * Only write non-dirop files that are in the pageout queue.
	 * We're very conservative about what we write; we want to be
	 * fast and async.
	 */
	mutex_enter(&lfs_lock);
    top:
	for (ip = TAILQ_FIRST(&fs->lfs_pchainhd); ip != NULL; ip = nip) {
		nip = TAILQ_NEXT(ip, i_lfs_pchain);
		vp = ITOV(ip);

		if (!(ip->i_flags & IN_PAGING))
			goto top;

		mutex_enter(vp->v_interlock);
		if ((vp->v_iflag & VI_XLOCK) || (vp->v_uflag & VU_DIROP) != 0) {
			mutex_exit(vp->v_interlock);
			continue;
		}
		if (vp->v_type != VREG) {
			mutex_exit(vp->v_interlock);
			continue;
		}
		if (lfs_vref(vp))
			continue;
		mutex_exit(&lfs_lock);

		if (vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_RETRY) != 0) {
			lfs_vunref(vp);
			mutex_enter(&lfs_lock);
			continue;
		}

		error = lfs_writefile(fs, sp, vp);
		if (!VPISEMPTY(vp) && !WRITEINPROG(vp) &&
		    !(ip->i_flag & IN_ALLMOD)) {
		    	mutex_enter(&lfs_lock);
			LFS_SET_UINO(ip, IN_MODIFIED);
		    	mutex_exit(&lfs_lock);
		}
		KDASSERT(ip->i_number != LFS_IFILE_INUM);
		(void) lfs_writeinode(fs, sp, ip);

		VOP_UNLOCK(vp);
		lfs_vunref(vp);

		if (error == EAGAIN) {
			lfs_writeseg(fs, sp);
			mutex_enter(&lfs_lock);
			break;
		}
		mutex_enter(&lfs_lock);
	}
	mutex_exit(&lfs_lock);
	(void) lfs_writeseg(fs, sp);
	lfs_segunlock(fs);
}

/*
 * Provide a fcntl interface to sys_lfs_{segwait,bmapv,markv}.
 */
int
lfs_fcntl(void *v)
{
	struct vop_fcntl_args /* {
		struct vnode *a_vp;
		u_int a_command;
		void * a_data;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct timeval tv;
	struct timeval *tvp;
	BLOCK_INFO *blkiov;
	CLEANERINFO *cip;
	SEGUSE *sup;
	int blkcnt, error, oclean;
	size_t fh_size;
	struct lfs_fcntl_markv blkvp;
	struct lwp *l;
	fsid_t *fsidp;
	struct lfs *fs;
	struct buf *bp;
	fhandle_t *fhp;
	daddr_t off;

	/* Only respect LFS fcntls on fs root or Ifile */
	if (VTOI(ap->a_vp)->i_number != ROOTINO &&
	    VTOI(ap->a_vp)->i_number != LFS_IFILE_INUM) {
		return ufs_fcntl(v);
	}

	/* Avoid locking a draining lock */
	if (ap->a_vp->v_mount->mnt_iflag & IMNT_UNMOUNT) {
		return ESHUTDOWN;
	}

	/* LFS control and monitoring fcntls are available only to root */
	l = curlwp;
	if (((ap->a_command & 0xff00) >> 8) == 'L' &&
	    (error = kauth_authorize_generic(l->l_cred, KAUTH_GENERIC_ISSUSER,
					     NULL)) != 0)
		return (error);

	fs = VTOI(ap->a_vp)->i_lfs;
	fsidp = &ap->a_vp->v_mount->mnt_stat.f_fsidx;

	error = 0;
	switch ((int)ap->a_command) {
	    case LFCNSEGWAITALL_COMPAT_50:
	    case LFCNSEGWAITALL_COMPAT:
		fsidp = NULL;
		/* FALLSTHROUGH */
	    case LFCNSEGWAIT_COMPAT_50:
	    case LFCNSEGWAIT_COMPAT:
		{
			struct timeval50 *tvp50
				= (struct timeval50 *)ap->a_data;
			timeval50_to_timeval(tvp50, &tv);
			tvp = &tv;
		}
		goto segwait_common;
	    case LFCNSEGWAITALL:
		fsidp = NULL;
		/* FALLSTHROUGH */
	    case LFCNSEGWAIT:
		tvp = (struct timeval *)ap->a_data;
segwait_common:
		mutex_enter(&lfs_lock);
		++fs->lfs_sleepers;
		mutex_exit(&lfs_lock);

		error = lfs_segwait(fsidp, tvp);

		mutex_enter(&lfs_lock);
		if (--fs->lfs_sleepers == 0)
			wakeup(&fs->lfs_sleepers);
		mutex_exit(&lfs_lock);
		return error;

	    case LFCNBMAPV:
	    case LFCNMARKV:
		blkvp = *(struct lfs_fcntl_markv *)ap->a_data;

		blkcnt = blkvp.blkcnt;
		if ((u_int) blkcnt > LFS_MARKV_MAXBLKCNT)
			return (EINVAL);
		blkiov = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO), LFS_NB_BLKIOV);
		if ((error = copyin(blkvp.blkiov, blkiov,
		     blkcnt * sizeof(BLOCK_INFO))) != 0) {
			lfs_free(fs, blkiov, LFS_NB_BLKIOV);
			return error;
		}

		mutex_enter(&lfs_lock);
		++fs->lfs_sleepers;
		mutex_exit(&lfs_lock);
		if (ap->a_command == LFCNBMAPV)
			error = lfs_bmapv(l->l_proc, fsidp, blkiov, blkcnt);
		else /* LFCNMARKV */
			error = lfs_markv(l->l_proc, fsidp, blkiov, blkcnt);
		if (error == 0)
			error = copyout(blkiov, blkvp.blkiov,
					blkcnt * sizeof(BLOCK_INFO));
		mutex_enter(&lfs_lock);
		if (--fs->lfs_sleepers == 0)
			wakeup(&fs->lfs_sleepers);
		mutex_exit(&lfs_lock);
		lfs_free(fs, blkiov, LFS_NB_BLKIOV);
		return error;

	    case LFCNRECLAIM:
		/*
		 * Flush dirops and write Ifile, allowing empty segments
		 * to be immediately reclaimed.
		 */
		lfs_writer_enter(fs, "pndirop");
		off = fs->lfs_offset;
		lfs_seglock(fs, SEGM_FORCE_CKP | SEGM_CKP);
		lfs_flush_dirops(fs);
		LFS_CLEANERINFO(cip, fs, bp);
		oclean = cip->clean;
		LFS_SYNC_CLEANERINFO(cip, fs, bp, 1);
		lfs_segwrite(ap->a_vp->v_mount, SEGM_FORCE_CKP);
		fs->lfs_sp->seg_flags |= SEGM_PROT;
		lfs_segunlock(fs);
		lfs_writer_leave(fs);

#ifdef DEBUG
		LFS_CLEANERINFO(cip, fs, bp);
		DLOG((DLOG_CLEAN, "lfs_fcntl: reclaim wrote %" PRId64
		      " blocks, cleaned %" PRId32 " segments (activesb %d)\n",
		      fs->lfs_offset - off, cip->clean - oclean,
		      fs->lfs_activesb));
		LFS_SYNC_CLEANERINFO(cip, fs, bp, 0);
#endif

		return 0;

	    case LFCNIFILEFH_COMPAT:
		/* Return the filehandle of the Ifile */
		if ((error = kauth_authorize_system(l->l_cred,
		    KAUTH_SYSTEM_FILEHANDLE, 0, NULL, NULL, NULL)) != 0)
			return (error);
		fhp = (struct fhandle *)ap->a_data;
		fhp->fh_fsid = *fsidp;
		fh_size = 16;	/* former VFS_MAXFIDSIZ */
		return lfs_vptofh(fs->lfs_ivnode, &(fhp->fh_fid), &fh_size);

	    case LFCNIFILEFH_COMPAT2:
	    case LFCNIFILEFH:
		/* Return the filehandle of the Ifile */
		fhp = (struct fhandle *)ap->a_data;
		fhp->fh_fsid = *fsidp;
		fh_size = sizeof(struct lfs_fhandle) -
		    offsetof(fhandle_t, fh_fid);
		return lfs_vptofh(fs->lfs_ivnode, &(fhp->fh_fid), &fh_size);

	    case LFCNREWIND:
		/* Move lfs_offset to the lowest-numbered segment */
		return lfs_rewind(fs, *(int *)ap->a_data);

	    case LFCNINVAL:
		/* Mark a segment SEGUSE_INVAL */
		LFS_SEGENTRY(sup, fs, *(int *)ap->a_data, bp);
		if (sup->su_nbytes > 0) {
			brelse(bp, 0);
			lfs_unset_inval_all(fs);
			return EBUSY;
		}
		sup->su_flags |= SEGUSE_INVAL;
		VOP_BWRITE(bp->b_vp, bp);
		return 0;

	    case LFCNRESIZE:
		/* Resize the filesystem */
		return lfs_resize_fs(fs, *(int *)ap->a_data);

	    case LFCNWRAPSTOP:
	    case LFCNWRAPSTOP_COMPAT:
		/*
		 * Hold lfs_newseg at segment 0; if requested, sleep until
		 * the filesystem wraps around.  To support external agents
		 * (dump, fsck-based regression test) that need to look at
		 * a snapshot of the filesystem, without necessarily
		 * requiring that all fs activity stops.
		 */
		if (fs->lfs_stoplwp == curlwp)
			return EALREADY;

		mutex_enter(&lfs_lock);
		while (fs->lfs_stoplwp != NULL)
			cv_wait(&fs->lfs_stopcv, &lfs_lock);
		fs->lfs_stoplwp = curlwp;
		if (fs->lfs_nowrap == 0)
			log(LOG_NOTICE, "%s: disabled log wrap\n", fs->lfs_fsmnt);
		++fs->lfs_nowrap;
		if (*(int *)ap->a_data == 1
		    || ap->a_command == LFCNWRAPSTOP_COMPAT) {
			log(LOG_NOTICE, "LFCNSTOPWRAP waiting for log wrap\n");
			error = mtsleep(&fs->lfs_nowrap, PCATCH | PUSER,
				"segwrap", 0, &lfs_lock);
			log(LOG_NOTICE, "LFCNSTOPWRAP done waiting\n");
			if (error) {
				lfs_wrapgo(fs, VTOI(ap->a_vp), 0);
			}
		}
		mutex_exit(&lfs_lock);
		return 0;

	    case LFCNWRAPGO:
	    case LFCNWRAPGO_COMPAT:
		/*
		 * Having done its work, the agent wakes up the writer.
		 * If the argument is 1, it sleeps until a new segment
		 * is selected.
		 */
		mutex_enter(&lfs_lock);
		error = lfs_wrapgo(fs, VTOI(ap->a_vp),
				   ap->a_command == LFCNWRAPGO_COMPAT ? 1 :
				    *((int *)ap->a_data));
		mutex_exit(&lfs_lock);
		return error;

	    case LFCNWRAPPASS:
		if ((VTOI(ap->a_vp)->i_lfs_iflags & LFSI_WRAPWAIT))
			return EALREADY;
		mutex_enter(&lfs_lock);
		if (fs->lfs_stoplwp != curlwp) {
			mutex_exit(&lfs_lock);
			return EALREADY;
		}
		if (fs->lfs_nowrap == 0) {
			mutex_exit(&lfs_lock);
			return EBUSY;
		}
		fs->lfs_wrappass = 1;
		wakeup(&fs->lfs_wrappass);
		/* Wait for the log to wrap, if asked */
		if (*(int *)ap->a_data) {
			mutex_enter(ap->a_vp->v_interlock);
			lfs_vref(ap->a_vp);
			VTOI(ap->a_vp)->i_lfs_iflags |= LFSI_WRAPWAIT;
			log(LOG_NOTICE, "LFCNPASS waiting for log wrap\n");
			error = mtsleep(&fs->lfs_nowrap, PCATCH | PUSER,
				"segwrap", 0, &lfs_lock);
			log(LOG_NOTICE, "LFCNPASS done waiting\n");
			VTOI(ap->a_vp)->i_lfs_iflags &= ~LFSI_WRAPWAIT;
			lfs_vunref(ap->a_vp);
		}
		mutex_exit(&lfs_lock);
		return error;

	    case LFCNWRAPSTATUS:
		mutex_enter(&lfs_lock);
		*(int *)ap->a_data = fs->lfs_wrapstatus;
		mutex_exit(&lfs_lock);
		return 0;

	    default:
		return ufs_fcntl(v);
	}
	return 0;
}

int
lfs_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	if (VTOI(ap->a_vp)->i_number == LFS_IFILE_INUM &&
	    (ap->a_access_type & VM_PROT_WRITE) != 0) {
		return EPERM;
	}
	if ((ap->a_access_type & VM_PROT_WRITE) != 0) {
		mutex_enter(&lfs_lock);
		LFS_SET_UINO(VTOI(ap->a_vp), IN_MODIFIED);
		mutex_exit(&lfs_lock);
	}

	/*
	 * we're relying on the fact that genfs_getpages() always read in
	 * entire filesystem blocks.
	 */
	return genfs_getpages(v);
}

/*
 * Wait for a page to become unbusy, possibly printing diagnostic messages
 * as well.
 *
 * Called with vp->v_interlock held; return with it held.
 */
static void
wait_for_page(struct vnode *vp, struct vm_page *pg, const char *label)
{
	if ((pg->flags & PG_BUSY) == 0)
		return;		/* Nothing to wait for! */

#if defined(DEBUG) && defined(UVM_PAGE_TRKOWN)
	static struct vm_page *lastpg;

	if (label != NULL && pg != lastpg) {
		if (pg->owner_tag) {
			printf("lfs_putpages[%d.%d]: %s: page %p owner %d.%d [%s]\n",
			       curproc->p_pid, curlwp->l_lid, label,
			       pg, pg->owner, pg->lowner, pg->owner_tag);
		} else {
			printf("lfs_putpages[%d.%d]: %s: page %p unowned?!\n",
			       curproc->p_pid, curlwp->l_lid, label, pg);
		}
	}
	lastpg = pg;
#endif

	pg->flags |= PG_WANTED;
	UVM_UNLOCK_AND_WAIT(pg, vp->v_interlock, 0, "lfsput", 0);
	mutex_enter(vp->v_interlock);
}

/*
 * This routine is called by lfs_putpages() when it can't complete the
 * write because a page is busy.  This means that either (1) someone,
 * possibly the pagedaemon, is looking at this page, and will give it up
 * presently; or (2) we ourselves are holding the page busy in the
 * process of being written (either gathered or actually on its way to
 * disk).  We don't need to give up the segment lock, but we might need
 * to call lfs_writeseg() to expedite the page's journey to disk.
 *
 * Called with vp->v_interlock held; return with it held.
 */
/* #define BUSYWAIT */
static void
write_and_wait(struct lfs *fs, struct vnode *vp, struct vm_page *pg,
	       int seglocked, const char *label)
{
#ifndef BUSYWAIT
	struct inode *ip = VTOI(vp);
	struct segment *sp = fs->lfs_sp;
	int count = 0;

	if (pg == NULL)
		return;

	while (pg->flags & PG_BUSY &&
	    pg->uobject == &vp->v_uobj) {
		mutex_exit(vp->v_interlock);
		if (sp->cbpp - sp->bpp > 1) {
			/* Write gathered pages */
			lfs_updatemeta(sp);
			lfs_release_finfo(fs);
			(void) lfs_writeseg(fs, sp);

			/*
			 * Reinitialize FIP
			 */
			KASSERT(sp->vp == vp);
			lfs_acquire_finfo(fs, ip->i_number,
					  ip->i_gen);
		}
		++count;
		mutex_enter(vp->v_interlock);
		wait_for_page(vp, pg, label);
	}
	if (label != NULL && count > 1)
		printf("lfs_putpages[%d]: %s: %sn = %d\n", curproc->p_pid,
		       label, (count > 0 ? "looping, " : ""), count);
#else
	preempt(1);
#endif
}

/*
 * Make sure that for all pages in every block in the given range,
 * either all are dirty or all are clean.  If any of the pages
 * we've seen so far are dirty, put the vnode on the paging chain,
 * and mark it IN_PAGING.
 *
 * If checkfirst != 0, don't check all the pages but return at the
 * first dirty page.
 */
static int
check_dirty(struct lfs *fs, struct vnode *vp,
	    off_t startoffset, off_t endoffset, off_t blkeof,
	    int flags, int checkfirst, struct vm_page **pgp)
{
	int by_list;
	struct vm_page *curpg = NULL; /* XXX: gcc */
	struct vm_page *pgs[MAXBSIZE / PAGE_SIZE], *pg;
	off_t soff = 0; /* XXX: gcc */
	voff_t off;
	int i;
	int nonexistent;
	int any_dirty;	/* number of dirty pages */
	int dirty;	/* number of dirty pages in a block */
	int tdirty;
	int pages_per_block = fs->lfs_bsize >> PAGE_SHIFT;
	int pagedaemon = (curlwp == uvm.pagedaemon_lwp);

	ASSERT_MAYBE_SEGLOCK(fs);
  top:
	by_list = (vp->v_uobj.uo_npages <=
		   ((endoffset - startoffset) >> PAGE_SHIFT) *
		   UVM_PAGE_TREE_PENALTY);
	any_dirty = 0;

	if (by_list) {
		curpg = TAILQ_FIRST(&vp->v_uobj.memq);
	} else {
		soff = startoffset;
	}
	while (by_list || soff < MIN(blkeof, endoffset)) {
		if (by_list) {
			/*
			 * Find the first page in a block.  Skip
			 * blocks outside our area of interest or beyond
			 * the end of file.
			 */
			KASSERT(curpg == NULL
			    || (curpg->flags & PG_MARKER) == 0);
			if (pages_per_block > 1) {
				while (curpg &&
				    ((curpg->offset & fs->lfs_bmask) ||
				    curpg->offset >= vp->v_size ||
				    curpg->offset >= endoffset)) {
					curpg = TAILQ_NEXT(curpg, listq.queue);
					KASSERT(curpg == NULL ||
					    (curpg->flags & PG_MARKER) == 0);
				}
			}
			if (curpg == NULL)
				break;
			soff = curpg->offset;
		}

		/*
		 * Mark all pages in extended range busy; find out if any
		 * of them are dirty.
		 */
		nonexistent = dirty = 0;
		for (i = 0; i == 0 || i < pages_per_block; i++) {
			if (by_list && pages_per_block <= 1) {
				pgs[i] = pg = curpg;
			} else {
				off = soff + (i << PAGE_SHIFT);
				pgs[i] = pg = uvm_pagelookup(&vp->v_uobj, off);
				if (pg == NULL) {
					++nonexistent;
					continue;
				}
			}
			KASSERT(pg != NULL);

			/*
			 * If we're holding the segment lock, we can deadlock
			 * against a process that has our page and is waiting
			 * for the cleaner, while the cleaner waits for the
			 * segment lock.  Just bail in that case.
			 */
			if ((pg->flags & PG_BUSY) &&
			    (pagedaemon || LFS_SEGLOCK_HELD(fs))) {
				if (i > 0)
					uvm_page_unbusy(pgs, i);
				DLOG((DLOG_PAGE, "lfs_putpages: avoiding 3-way or pagedaemon deadlock\n"));
				if (pgp)
					*pgp = pg;
				return -1;
			}

			while (pg->flags & PG_BUSY) {
				wait_for_page(vp, pg, NULL);
				if (i > 0)
					uvm_page_unbusy(pgs, i);
				goto top;
			}
			pg->flags |= PG_BUSY;
			UVM_PAGE_OWN(pg, "lfs_putpages");

			pmap_page_protect(pg, VM_PROT_NONE);
			tdirty = (pmap_clear_modify(pg) ||
				  (pg->flags & PG_CLEAN) == 0);
			dirty += tdirty;
		}
		if (pages_per_block > 0 && nonexistent >= pages_per_block) {
			if (by_list) {
				curpg = TAILQ_NEXT(curpg, listq.queue);
			} else {
				soff += fs->lfs_bsize;
			}
			continue;
		}

		any_dirty += dirty;
		KASSERT(nonexistent == 0);

		/*
		 * If any are dirty make all dirty; unbusy them,
		 * but if we were asked to clean, wire them so that
		 * the pagedaemon doesn't bother us about them while
		 * they're on their way to disk.
		 */
		for (i = 0; i == 0 || i < pages_per_block; i++) {
			pg = pgs[i];
			KASSERT(!((pg->flags & PG_CLEAN) && (pg->flags & PG_DELWRI)));
			if (dirty) {
				pg->flags &= ~PG_CLEAN;
				if (flags & PGO_FREE) {
					/*
					 * Wire the page so that
					 * pdaemon doesn't see it again.
					 */
					mutex_enter(&uvm_pageqlock);
					uvm_pagewire(pg);
					mutex_exit(&uvm_pageqlock);

					/* Suspended write flag */
					pg->flags |= PG_DELWRI;
				}
			}
			if (pg->flags & PG_WANTED)
				wakeup(pg);
			pg->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pg, NULL);
		}

		if (checkfirst && any_dirty)
			break;

		if (by_list) {
			curpg = TAILQ_NEXT(curpg, listq.queue);
		} else {
			soff += MAX(PAGE_SIZE, fs->lfs_bsize);
		}
	}

	return any_dirty;
}

/*
 * lfs_putpages functions like genfs_putpages except that
 *
 * (1) It needs to bounds-check the incoming requests to ensure that
 *     they are block-aligned; if they are not, expand the range and
 *     do the right thing in case, e.g., the requested range is clean
 *     but the expanded range is dirty.
 *
 * (2) It needs to explicitly send blocks to be written when it is done.
 *     If VOP_PUTPAGES is called without the seglock held, we simply take
 *     the seglock and let lfs_segunlock wait for us.
 *     XXX There might be a bad situation if we have to flush a vnode while
 *     XXX lfs_markv is in operation.  As of this writing we panic in this
 *     XXX case.
 *
 * Assumptions:
 *
 * (1) The caller does not hold any pages in this vnode busy.  If it does,
 *     there is a danger that when we expand the page range and busy the
 *     pages we will deadlock.
 *
 * (2) We are called with vp->v_interlock held; we must return with it
 *     released.
 *
 * (3) We don't absolutely have to free pages right away, provided that
 *     the request does not have PGO_SYNCIO.  When the pagedaemon gives
 *     us a request with PGO_FREE, we take the pages out of the paging
 *     queue and wake up the writer, which will handle freeing them for us.
 *
 *     We ensure that for any filesystem block, all pages for that
 *     block are either resident or not, even if those pages are higher
 *     than EOF; that means that we will be getting requests to free
 *     "unused" pages above EOF all the time, and should ignore them.
 *
 * (4) If we are called with PGO_LOCKED, the finfo array we are to write
 *     into has been set up for us by lfs_writefile.  If not, we will
 *     have to handle allocating and/or freeing an finfo entry.
 *
 * XXX note that we're (ab)using PGO_LOCKED as "seglock held".
 */

/* How many times to loop before we should start to worry */
#define TOOMANY 4

int
lfs_putpages(void *v)
{
	int error;
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp;
	struct inode *ip;
	struct lfs *fs;
	struct segment *sp;
	off_t origoffset, startoffset, endoffset, origendoffset, blkeof;
	off_t off, max_endoffset;
	bool seglocked, sync, pagedaemon;
	struct vm_page *pg, *busypg;
	UVMHIST_FUNC("lfs_putpages"); UVMHIST_CALLED(ubchist);
#ifdef DEBUG
	int debug_n_again, debug_n_dirtyclean;
#endif

	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ip->i_lfs;
	sync = (ap->a_flags & PGO_SYNCIO) != 0;
	pagedaemon = (curlwp == uvm.pagedaemon_lwp);

	/* Putpages does nothing for metadata. */
	if (vp == fs->lfs_ivnode || vp->v_type != VREG) {
		mutex_exit(vp->v_interlock);
		return 0;
	}

	/*
	 * If there are no pages, don't do anything.
	 */
	if (vp->v_uobj.uo_npages == 0) {
		if (TAILQ_EMPTY(&vp->v_uobj.memq) &&
		    (vp->v_iflag & VI_ONWORKLST) &&
		    LIST_FIRST(&vp->v_dirtyblkhd) == NULL) {
			vp->v_iflag &= ~VI_WRMAPDIRTY;
			vn_syncer_remove_from_worklist(vp);
		}
		mutex_exit(vp->v_interlock);
		
		/* Remove us from paging queue, if we were on it */
		mutex_enter(&lfs_lock);
		if (ip->i_flags & IN_PAGING) {
			ip->i_flags &= ~IN_PAGING;
			TAILQ_REMOVE(&fs->lfs_pchainhd, ip, i_lfs_pchain);
		}
		mutex_exit(&lfs_lock);
		return 0;
	}

	blkeof = blkroundup(fs, ip->i_size);

	/*
	 * Ignore requests to free pages past EOF but in the same block
	 * as EOF, unless the request is synchronous.  (If the request is
	 * sync, it comes from lfs_truncate.)
	 * XXXUBC Make these pages look "active" so the pagedaemon won't
	 * XXXUBC bother us with them again.
	 */
	if (!sync && ap->a_offlo >= ip->i_size && ap->a_offlo < blkeof) {
		origoffset = ap->a_offlo;
		for (off = origoffset; off < blkeof; off += fs->lfs_bsize) {
			pg = uvm_pagelookup(&vp->v_uobj, off);
			KASSERT(pg != NULL);
			while (pg->flags & PG_BUSY) {
				pg->flags |= PG_WANTED;
				UVM_UNLOCK_AND_WAIT(pg, vp->v_interlock, 0,
						    "lfsput2", 0);
				mutex_enter(vp->v_interlock);
			}
			mutex_enter(&uvm_pageqlock);
			uvm_pageactivate(pg);
			mutex_exit(&uvm_pageqlock);
		}
		ap->a_offlo = blkeof;
		if (ap->a_offhi > 0 && ap->a_offhi <= ap->a_offlo) {
			mutex_exit(vp->v_interlock);
			return 0;
		}
	}

	/*
	 * Extend page range to start and end at block boundaries.
	 * (For the purposes of VOP_PUTPAGES, fragments don't exist.)
	 */
	origoffset = ap->a_offlo;
	origendoffset = ap->a_offhi;
	startoffset = origoffset & ~(fs->lfs_bmask);
	max_endoffset = (trunc_page(LLONG_MAX) >> fs->lfs_bshift)
					       << fs->lfs_bshift;

	if (origendoffset == 0 || ap->a_flags & PGO_ALLPAGES) {
		endoffset = max_endoffset;
		origendoffset = endoffset;
	} else {
		origendoffset = round_page(ap->a_offhi);
		endoffset = round_page(blkroundup(fs, origendoffset));
	}

	KASSERT(startoffset > 0 || endoffset >= startoffset);
	if (startoffset == endoffset) {
		/* Nothing to do, why were we called? */
		mutex_exit(vp->v_interlock);
		DLOG((DLOG_PAGE, "lfs_putpages: startoffset = endoffset = %"
		      PRId64 "\n", startoffset));
		return 0;
	}

	ap->a_offlo = startoffset;
	ap->a_offhi = endoffset;

	/*
	 * If not cleaning, just send the pages through genfs_putpages
	 * to be returned to the pool.
	 */
	if (!(ap->a_flags & PGO_CLEANIT))
		return genfs_putpages(v);

	/* Set PGO_BUSYFAIL to avoid deadlocks */
	ap->a_flags |= PGO_BUSYFAIL;

	/*
	 * Likewise, if we are asked to clean but the pages are not
	 * dirty, we can just free them using genfs_putpages.
	 */
#ifdef DEBUG
	debug_n_dirtyclean = 0;
#endif
	do {
		int r;

		/* Count the number of dirty pages */
		r = check_dirty(fs, vp, startoffset, endoffset, blkeof,
				ap->a_flags, 1, NULL);
		if (r < 0) {
			/* Pages are busy with another process */
			mutex_exit(vp->v_interlock);
			return EDEADLK;
		}
		if (r > 0) /* Some pages are dirty */
			break;

		/*
		 * Sometimes pages are dirtied between the time that
		 * we check and the time we try to clean them.
		 * Instruct lfs_gop_write to return EDEADLK in this case
		 * so we can write them properly.
		 */
		ip->i_lfs_iflags |= LFSI_NO_GOP_WRITE;
		r = genfs_do_putpages(vp, startoffset, endoffset,
				       ap->a_flags & ~PGO_SYNCIO, &busypg);
		ip->i_lfs_iflags &= ~LFSI_NO_GOP_WRITE;
		if (r != EDEADLK)
			return r;

		/* One of the pages was busy.  Start over. */
		mutex_enter(vp->v_interlock);
		wait_for_page(vp, busypg, "dirtyclean");
#ifdef DEBUG
		++debug_n_dirtyclean;
#endif
	} while(1);

#ifdef DEBUG
	if (debug_n_dirtyclean > TOOMANY)
		printf("lfs_putpages: dirtyclean: looping, n = %d\n",
		       debug_n_dirtyclean);
#endif

	/*
	 * Dirty and asked to clean.
	 *
	 * Pagedaemon can't actually write LFS pages; wake up
	 * the writer to take care of that.  The writer will
	 * notice the pager inode queue and act on that.
	 *
	 * XXX We must drop the vp->interlock before taking the lfs_lock or we
	 * get a nasty deadlock with lfs_flush_pchain().
	 */
	if (pagedaemon) {
		mutex_exit(vp->v_interlock);
		mutex_enter(&lfs_lock);
		if (!(ip->i_flags & IN_PAGING)) {
			ip->i_flags |= IN_PAGING;
			TAILQ_INSERT_TAIL(&fs->lfs_pchainhd, ip, i_lfs_pchain);
		} 
		wakeup(&lfs_writer_daemon);
		mutex_exit(&lfs_lock);
		preempt();
		return EWOULDBLOCK;
	}

	/*
	 * If this is a file created in a recent dirop, we can't flush its
	 * inode until the dirop is complete.  Drain dirops, then flush the
	 * filesystem (taking care of any other pending dirops while we're
	 * at it).
	 */
	if ((ap->a_flags & (PGO_CLEANIT|PGO_LOCKED)) == PGO_CLEANIT &&
	    (vp->v_uflag & VU_DIROP)) {
		int locked;

		DLOG((DLOG_PAGE, "lfs_putpages: flushing VU_DIROP\n"));
		/* XXX VOP_ISLOCKED() may not be used for lock decisions. */
		locked = (VOP_ISLOCKED(vp) == LK_EXCLUSIVE);
		mutex_exit(vp->v_interlock);
		lfs_writer_enter(fs, "ppdirop");
		if (locked)
			VOP_UNLOCK(vp); /* XXX why? */

		mutex_enter(&lfs_lock);
		lfs_flush_fs(fs, sync ? SEGM_SYNC : 0);
		mutex_exit(&lfs_lock);

		if (locked)
			VOP_LOCK(vp, LK_EXCLUSIVE);
		mutex_enter(vp->v_interlock);
		lfs_writer_leave(fs);

		/* XXX the flush should have taken care of this one too! */
	}

	/*
	 * This is it.	We are going to write some pages.  From here on
	 * down it's all just mechanics.
	 *
	 * Don't let genfs_putpages wait; lfs_segunlock will wait for us.
	 */
	ap->a_flags &= ~PGO_SYNCIO;

	/*
	 * If we've already got the seglock, flush the node and return.
	 * The FIP has already been set up for us by lfs_writefile,
	 * and FIP cleanup and lfs_updatemeta will also be done there,
	 * unless genfs_putpages returns EDEADLK; then we must flush
	 * what we have, and correct FIP and segment header accounting.
	 */
  get_seglock:
	/*
	 * If we are not called with the segment locked, lock it.
	 * Account for a new FIP in the segment header, and set sp->vp.
	 * (This should duplicate the setup at the top of lfs_writefile().)
	 */
	seglocked = (ap->a_flags & PGO_LOCKED) != 0;
	if (!seglocked) {
		mutex_exit(vp->v_interlock);
		error = lfs_seglock(fs, SEGM_PROT | (sync ? SEGM_SYNC : 0));
		if (error != 0)
			return error;
		mutex_enter(vp->v_interlock);
		lfs_acquire_finfo(fs, ip->i_number, ip->i_gen);
	}
	sp = fs->lfs_sp;
	KASSERT(sp->vp == NULL);
	sp->vp = vp;

	/*
	 * Ensure that the partial segment is marked SS_DIROP if this
	 * vnode is a DIROP.
	 */
	if (!seglocked && vp->v_uflag & VU_DIROP)
		((SEGSUM *)(sp->segsum))->ss_flags |= (SS_DIROP|SS_CONT);

	/*
	 * Loop over genfs_putpages until all pages are gathered.
	 * genfs_putpages() drops the interlock, so reacquire it if necessary.
	 * Whenever we lose the interlock we have to rerun check_dirty, as
	 * well, since more pages might have been dirtied in our absence.
	 */
#ifdef DEBUG
	debug_n_again = 0;
#endif
	do {
		busypg = NULL;
		if (check_dirty(fs, vp, startoffset, endoffset, blkeof,
				ap->a_flags, 0, &busypg) < 0) {
			mutex_exit(vp->v_interlock);

			mutex_enter(vp->v_interlock);
			write_and_wait(fs, vp, busypg, seglocked, NULL);
			if (!seglocked) {
				mutex_exit(vp->v_interlock);
				lfs_release_finfo(fs);
				lfs_segunlock(fs);
				mutex_enter(vp->v_interlock);
			}
			sp->vp = NULL;
			goto get_seglock;
		}
	
		busypg = NULL;
		error = genfs_do_putpages(vp, startoffset, endoffset,
					   ap->a_flags, &busypg);
	
		if (error == EDEADLK || error == EAGAIN) {
			DLOG((DLOG_PAGE, "lfs_putpages: genfs_putpages returned"
			      " %d ino %d off %x (seg %d)\n", error,
			      ip->i_number, fs->lfs_offset,
			      dtosn(fs, fs->lfs_offset)));

			mutex_enter(vp->v_interlock);
			write_and_wait(fs, vp, busypg, seglocked, "again");
		}
#ifdef DEBUG
		++debug_n_again;
#endif
	} while (error == EDEADLK);
#ifdef DEBUG
	if (debug_n_again > TOOMANY)
		printf("lfs_putpages: again: looping, n = %d\n", debug_n_again);
#endif

	KASSERT(sp != NULL && sp->vp == vp);
	if (!seglocked) {
		sp->vp = NULL;

		/* Write indirect blocks as well */
		lfs_gather(fs, fs->lfs_sp, vp, lfs_match_indir);
		lfs_gather(fs, fs->lfs_sp, vp, lfs_match_dindir);
		lfs_gather(fs, fs->lfs_sp, vp, lfs_match_tindir);

		KASSERT(sp->vp == NULL);
		sp->vp = vp;
	}

	/*
	 * Blocks are now gathered into a segment waiting to be written.
	 * All that's left to do is update metadata, and write them.
	 */
	lfs_updatemeta(sp);
	KASSERT(sp->vp == vp);
	sp->vp = NULL;

	/*
	 * If we were called from lfs_writefile, we don't need to clean up
	 * the FIP or unlock the segment lock.	We're done.
	 */
	if (seglocked)
		return error;

	/* Clean up FIP and send it to disk. */
	lfs_release_finfo(fs);
	lfs_writeseg(fs, fs->lfs_sp);

	/*
	 * Remove us from paging queue if we wrote all our pages.
	 */
	if (origendoffset == 0 || ap->a_flags & PGO_ALLPAGES) {
		mutex_enter(&lfs_lock);
		if (ip->i_flags & IN_PAGING) {
			ip->i_flags &= ~IN_PAGING;
			TAILQ_REMOVE(&fs->lfs_pchainhd, ip, i_lfs_pchain);
		}
		mutex_exit(&lfs_lock);
	}

	/*
	 * XXX - with the malloc/copy writeseg, the pages are freed by now
	 * even if we don't wait (e.g. if we hold a nested lock).  This
	 * will not be true if we stop using malloc/copy.
	 */
	KASSERT(fs->lfs_sp->seg_flags & SEGM_PROT);
	lfs_segunlock(fs);

	/*
	 * Wait for v_numoutput to drop to zero.  The seglock should
	 * take care of this, but there is a slight possibility that
	 * aiodoned might not have got around to our buffers yet.
	 */
	if (sync) {
		mutex_enter(vp->v_interlock);
		while (vp->v_numoutput > 0) {
			DLOG((DLOG_PAGE, "lfs_putpages: ino %d sleeping on"
			      " num %d\n", ip->i_number, vp->v_numoutput));
			cv_wait(&vp->v_cv, vp->v_interlock);
		}
		mutex_exit(vp->v_interlock);
	}
	return error;
}

/*
 * Return the last logical file offset that should be written for this file
 * if we're doing a write that ends at "size".	If writing, we need to know
 * about sizes on disk, i.e. fragments if there are any; if reading, we need
 * to know about entire blocks.
 */
void
lfs_gop_size(struct vnode *vp, off_t size, off_t *eobp, int flags)
{
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;
	daddr_t olbn, nlbn;

	olbn = lblkno(fs, ip->i_size);
	nlbn = lblkno(fs, size);
	if (!(flags & GOP_SIZE_MEM) && nlbn < NDADDR && olbn <= nlbn) {
		*eobp = fragroundup(fs, size);
	} else {
		*eobp = blkroundup(fs, size);
	}
}

#ifdef DEBUG
void lfs_dump_vop(void *);

void
lfs_dump_vop(void *v)
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;

#ifdef DDB
	vfs_vnode_print(ap->a_vp, 0, printf);
#endif
	lfs_dump_dinode(VTOI(ap->a_vp)->i_din.ffs1_din);
}
#endif

int
lfs_mmap(void *v)
{
	struct vop_mmap_args /* {
		const struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		vm_prot_t a_prot;
		kauth_cred_t a_cred;
	} */ *ap = v;

	if (VTOI(ap->a_vp)->i_number == LFS_IFILE_INUM)
		return EOPNOTSUPP;
	return ufs_mmap(v);
}
