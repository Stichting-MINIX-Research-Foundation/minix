/*	$NetBSD: lfs_vnops.c,v 1.293 2015/09/21 01:24:23 dholland Exp $	*/

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

/*  from NetBSD: ufs_vnops.c,v 1.213 2013/06/08 05:47:02 kardel Exp  */
/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
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
 * Copyright (c) 1982, 1986, 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_vnops.c	8.28 (Berkeley) 7/31/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_vnops.c,v 1.293 2015/09/21 01:24:23 dholland Exp $");

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

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_bswap.h>
#include <ufs/lfs/ulfs_extern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_pmap.h>
#include <uvm/uvm_stat.h>
#include <uvm/uvm_pager.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_kernel.h>
#include <ufs/lfs/lfs_extern.h>

extern pid_t lfs_writer_daemon;
int lfs_ignore_lazy_sync = 1;

static int lfs_openextattr(void *v);
static int lfs_closeextattr(void *v);
static int lfs_getextattr(void *v);
static int lfs_setextattr(void *v);
static int lfs_listextattr(void *v);
static int lfs_deleteextattr(void *v);

/* Global vfs data structures for lfs. */
int (**lfs_vnodeop_p)(void *);
const struct vnodeopv_entry_desc lfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, ulfs_lookup },		/* lookup */
	{ &vop_create_desc, lfs_create },		/* create */
	{ &vop_whiteout_desc, ulfs_whiteout },		/* whiteout */
	{ &vop_mknod_desc, lfs_mknod },			/* mknod */
	{ &vop_open_desc, ulfs_open },			/* open */
	{ &vop_close_desc, lfs_close },			/* close */
	{ &vop_access_desc, ulfs_access },		/* access */
	{ &vop_getattr_desc, lfs_getattr },		/* getattr */
	{ &vop_setattr_desc, lfs_setattr },		/* setattr */
	{ &vop_read_desc, lfs_read },			/* read */
	{ &vop_write_desc, lfs_write },			/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_ioctl_desc, ulfs_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, lfs_fcntl },			/* fcntl */
	{ &vop_poll_desc, ulfs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, ulfs_revoke },		/* revoke */
	{ &vop_mmap_desc, lfs_mmap },			/* mmap */
	{ &vop_fsync_desc, lfs_fsync },			/* fsync */
	{ &vop_seek_desc, ulfs_seek },			/* seek */
	{ &vop_remove_desc, lfs_remove },		/* remove */
	{ &vop_link_desc, lfs_link },			/* link */
	{ &vop_rename_desc, lfs_rename },		/* rename */
	{ &vop_mkdir_desc, lfs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, lfs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, lfs_symlink },		/* symlink */
	{ &vop_readdir_desc, ulfs_readdir },		/* readdir */
	{ &vop_readlink_desc, ulfs_readlink },		/* readlink */
	{ &vop_abortop_desc, ulfs_abortop },		/* abortop */
	{ &vop_inactive_desc, lfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, lfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ulfs_lock },			/* lock */
	{ &vop_unlock_desc, ulfs_unlock },		/* unlock */
	{ &vop_bmap_desc, ulfs_bmap },			/* bmap */
	{ &vop_strategy_desc, lfs_strategy },		/* strategy */
	{ &vop_print_desc, ulfs_print },		/* print */
	{ &vop_islocked_desc, ulfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ulfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ulfs_advlock },		/* advlock */
	{ &vop_bwrite_desc, lfs_bwrite },		/* bwrite */
	{ &vop_getpages_desc, lfs_getpages },		/* getpages */
	{ &vop_putpages_desc, lfs_putpages },		/* putpages */
	{ &vop_openextattr_desc, lfs_openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, lfs_closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, lfs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, lfs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, lfs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, lfs_deleteextattr },	/* deleteextattr */
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
	{ &vop_access_desc, ulfs_access },		/* access */
	{ &vop_getattr_desc, lfs_getattr },		/* getattr */
	{ &vop_setattr_desc, lfs_setattr },		/* setattr */
	{ &vop_read_desc, ulfsspec_read },		/* read */
	{ &vop_write_desc, ulfsspec_write },		/* write */
	{ &vop_fallocate_desc, spec_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, spec_fdiscard },		/* fdiscard */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, ulfs_fcntl },		/* fcntl */
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
	{ &vop_lock_desc, ulfs_lock },			/* lock */
	{ &vop_unlock_desc, ulfs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, ulfs_print },		/* print */
	{ &vop_islocked_desc, ulfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ &vop_openextattr_desc, lfs_openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, lfs_closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, lfs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, lfs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, lfs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, lfs_deleteextattr },	/* deleteextattr */
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
	{ &vop_access_desc, ulfs_access },		/* access */
	{ &vop_getattr_desc, lfs_getattr },		/* getattr */
	{ &vop_setattr_desc, lfs_setattr },		/* setattr */
	{ &vop_read_desc, ulfsfifo_read },		/* read */
	{ &vop_write_desc, ulfsfifo_write },		/* write */
	{ &vop_fallocate_desc, vn_fifo_bypass },	/* fallocate */
	{ &vop_fdiscard_desc, vn_fifo_bypass },		/* fdiscard */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_fcntl_desc, ulfs_fcntl },		/* fcntl */
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
	{ &vop_lock_desc, ulfs_lock },			/* lock */
	{ &vop_unlock_desc, ulfs_unlock },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, ulfs_print },		/* print */
	{ &vop_islocked_desc, ulfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, lfs_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass },		/* putpages */
	{ &vop_openextattr_desc, lfs_openextattr },	/* openextattr */
	{ &vop_closeextattr_desc, lfs_closeextattr },	/* closeextattr */
	{ &vop_getextattr_desc, lfs_getextattr },	/* getextattr */
	{ &vop_setextattr_desc, lfs_setextattr },	/* setextattr */
	{ &vop_listextattr_desc, lfs_listextattr },	/* listextattr */
	{ &vop_deleteextattr_desc, lfs_deleteextattr },	/* deleteextattr */
	{ NULL, NULL }
};
const struct vnodeopv_desc lfs_fifoop_opv_desc =
	{ &lfs_fifoop_p, lfs_fifoop_entries };

#define	LFS_READWRITE
#include <ufs/lfs/ulfs_readwrite.c>
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
			mtsleep(&fs->lfs_availsleep, PCATCH | PUSER,
				"lfs_fsync", hz / 100 + 1, &lfs_lock);
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
 * Take IN_ADIROP off, then call ulfs_inactive.
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

#ifdef DEBUG
	/*
	 * This might happen on unmount.
	 * XXX If it happens at any other time, it should be a panic.
	 */
	if (ap->a_vp->v_uflag & VU_DIROP) {
		struct inode *ip = VTOI(ap->a_vp);
		printf("lfs_inactive: inactivating VU_DIROP? ino = %d\n", (int)ip->i_number);
	}
#endif /* DIAGNOSTIC */

	return ulfs_inactive(v);
}

int
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
	/* fs->lfs_doifile = 1; */ /* XXX why? --ks */
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
 * Opposite of lfs_set_dirop... mostly. For now at least must call
 * UNMARK_VNODE(dvp) explicitly first. (XXX: clean that up)
 */
void
lfs_unset_dirop(struct lfs *fs, struct vnode *dvp, const char *str)
{
	mutex_enter(&lfs_lock);
	--fs->lfs_dirops;
	if (!fs->lfs_dirops) {
		if (fs->lfs_nadirop) {
			panic("lfs_unset_dirop: %s: no dirops but "
			      " nadirop=%d", str,
			      fs->lfs_nadirop);
		}
		wakeup(&fs->lfs_writer);
		mutex_exit(&lfs_lock);
		lfs_check(dvp, LFS_UNUSED_LBN, 0);
	} else {
		mutex_exit(&lfs_lock);
	}
	lfs_reserve(fs, dvp, NULL, -LFS_NRESERVE(fs));
}

void
lfs_mark_vnode(struct vnode *vp)
{
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;

	mutex_enter(&lfs_lock);
	if (!(ip->i_flag & IN_ADIROP)) {
		if (!(vp->v_uflag & VU_DIROP)) {
			mutex_exit(&lfs_lock);
			vref(vp);
			mutex_enter(&lfs_lock);
			++lfs_dirvcount;
			++fs->lfs_dirvcount;
			TAILQ_INSERT_TAIL(&fs->lfs_dchainhd, ip, i_lfs_dchain);
			vp->v_uflag |= VU_DIROP;
		}
		++fs->lfs_nadirop;
		ip->i_flag &= ~IN_CDIROP;
		ip->i_flag |= IN_ADIROP;
	} else
		KASSERT(vp->v_uflag & VU_DIROP);
	mutex_exit(&lfs_lock);
}

void
lfs_unmark_vnode(struct vnode *vp)
{
	struct inode *ip = VTOI(vp);

	mutex_enter(&lfs_lock);
	if (ip && (ip->i_flag & IN_ADIROP)) {
		KASSERT(vp->v_uflag & VU_DIROP);
		--ip->i_lfs->lfs_nadirop;
		ip->i_flag &= ~IN_ADIROP;
	}
	mutex_exit(&lfs_lock);
}

int
lfs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct lfs *fs;
	struct vnode *dvp, **vpp;
	struct inode *ip;
	struct ulfs_lookup_results *ulr;
	ssize_t len; /* XXX should be size_t */
	int error;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;

	KASSERT(vpp != NULL);
	KASSERT(*vpp == NULL);
	KASSERT(ap->a_vap->va_type == VLNK);

	/* XXX should handle this material another way */
	ulr = &VTOI(ap->a_dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(ap->a_dvp));

	fs = VFSTOULFS(dvp->v_mount)->um_lfs;
	ASSERT_NO_SEGLOCK(fs);
	if (fs->lfs_ronly) {
		return EROFS;
	}

	error = lfs_set_dirop(dvp, NULL);
	if (error)
		return error;

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
	error = ulfs_makeinode(ap->a_vap, dvp, ulr, vpp, ap->a_cnp);
	if (error) {
		goto out;
	}

	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	ip = VTOI(*vpp);

	len = strlen(ap->a_target);
	if (len < ip->i_lfs->um_maxsymlinklen) {
		memcpy((char *)SHORTLINK(ip), ap->a_target, len);
		ip->i_size = len;
		DIP_ASSIGN(ip, size, len);
		uvm_vnp_setsize(*vpp, ip->i_size);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if ((*vpp)->v_mount->mnt_flag & MNT_RELATIME)
			ip->i_flag |= IN_ACCESS;
	} else {
		error = ulfs_bufio(UIO_WRITE, *vpp, ap->a_target, len, (off_t)0,
		    IO_NODELOCKED | IO_JOURNALLOCKED, ap->a_cnp->cn_cred, NULL,
		    NULL);
	}

	VOP_UNLOCK(*vpp);
	if (error)
		vrele(*vpp);

out:
	fstrans_done(dvp->v_mount);

	UNMARK_VNODE(dvp);
	/* XXX: is it even possible for the symlink to get MARK'd? */
	UNMARK_VNODE(*vpp);
	if (error) {
		*vpp = NULL;
	}
	lfs_unset_dirop(fs, dvp, "symlink");

	vrele(dvp);
	return (error);
}

int
lfs_mknod(void *v)
{
	struct vop_mknod_v3_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct lfs *fs;
	struct vnode *dvp, **vpp;
	struct vattr *vap;
	struct inode *ip;
	int error;
	ino_t		ino;
	struct ulfs_lookup_results *ulr;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	vap = ap->a_vap;

	KASSERT(vpp != NULL);
	KASSERT(*vpp == NULL);
	
	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fs = VFSTOULFS(dvp->v_mount)->um_lfs;
	ASSERT_NO_SEGLOCK(fs);
	if (fs->lfs_ronly) {
		return EROFS;
	}

	error = lfs_set_dirop(dvp, NULL);
	if (error)
		return error;

	fstrans_start(ap->a_dvp->v_mount, FSTRANS_SHARED);
	error = ulfs_makeinode(vap, dvp, ulr, vpp, ap->a_cnp);

	/* Either way we're done with the dirop at this point */
	UNMARK_VNODE(dvp);
	UNMARK_VNODE(*vpp);
	lfs_unset_dirop(fs, dvp, "mknod");
	/*
	 * XXX this is where this used to be (though inside some evil
	 * macros) but it clearly should be moved further down.
	 * - dholland 20140515
	 */
	vrele(dvp);

	if (error) {
		fstrans_done(ap->a_dvp->v_mount);
		*vpp = NULL;
		return (error);
	}

	VN_KNOTE(dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
	ino = ip->i_number;
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;

	/*
	 * Call fsync to write the vnode so that we don't have to deal with
	 * flushing it when it's marked VU_DIROP or reclaiming.
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

	fstrans_done(ap->a_dvp->v_mount);
	KASSERT(error == 0);
	VOP_UNLOCK(*vpp);
	return (0);
}

/*
 * Create a regular file
 */
int
lfs_create(void *v)
{
	struct vop_create_v3_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct lfs *fs;
	struct vnode *dvp, **vpp;
	struct vattr *vap;
	struct ulfs_lookup_results *ulr;
	int error;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	vap = ap->a_vap;

	KASSERT(vpp != NULL);
	KASSERT(*vpp == NULL);

	/* XXX should handle this material another way */
	ulr = &VTOI(dvp)->i_crap;
	ULFS_CHECK_CRAPCOUNTER(VTOI(dvp));

	fs = VFSTOULFS(dvp->v_mount)->um_lfs;
	ASSERT_NO_SEGLOCK(fs);
	if (fs->lfs_ronly) {
		return EROFS;
	}

	error = lfs_set_dirop(dvp, NULL);
	if (error)
		return error;

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);
	error = ulfs_makeinode(vap, dvp, ulr, vpp, ap->a_cnp);
	if (error) {
		fstrans_done(dvp->v_mount);
		goto out;
	}
	fstrans_done(dvp->v_mount);
	VN_KNOTE(dvp, NOTE_WRITE);
	VOP_UNLOCK(*vpp);

out:

	UNMARK_VNODE(dvp);
	UNMARK_VNODE(*vpp);
	if (error) {
		*vpp = NULL;
	}
	lfs_unset_dirop(fs, dvp, "create");

	vrele(dvp);
	return (error);
}

int
lfs_mkdir(void *v)
{
	struct vop_mkdir_v3_args	/* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct lfs *fs;
	struct vnode *dvp, *tvp, **vpp;
	struct inode *dp, *ip;
	struct componentname *cnp;
	struct vattr *vap;
	struct ulfs_lookup_results *ulr;
	struct buf *bp;
	LFS_DIRHEADER *dirp;
	int dirblksiz;
	int error;

	dvp = ap->a_dvp;
	tvp = NULL;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;
	vap = ap->a_vap;

	dp = VTOI(dvp);
	ip = NULL;

	KASSERT(vap->va_type == VDIR);
	KASSERT(vpp != NULL);
	KASSERT(*vpp == NULL);

	/* XXX should handle this material another way */
	ulr = &dp->i_crap;
	ULFS_CHECK_CRAPCOUNTER(dp);

	fs = VFSTOULFS(dvp->v_mount)->um_lfs;
	ASSERT_NO_SEGLOCK(fs);
	if (fs->lfs_ronly) {
		return EROFS;
	}
	dirblksiz = fs->um_dirblksiz;
	/* XXX dholland 20150911 I believe this to be true, but... */
	//KASSERT(dirblksiz == LFS_DIRBLKSIZ);

	error = lfs_set_dirop(dvp, NULL);
	if (error)
		return error;

	fstrans_start(dvp->v_mount, FSTRANS_SHARED);

	if ((nlink_t)dp->i_nlink >= LINK_MAX) {
		error = EMLINK;
		goto out;
	}

	/*
	 * Must simulate part of ulfs_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	error = vcache_new(dvp->v_mount, dvp, vap, cnp->cn_cred, ap->a_vpp);
	if (error)
		goto out;

	error = vn_lock(*ap->a_vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*ap->a_vpp);
		*ap->a_vpp = NULL;
		goto out;
	}

	tvp = *ap->a_vpp;
	lfs_mark_vnode(tvp);
	ip = VTOI(tvp);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_nlink = 2;
	DIP_ASSIGN(ip, nlink, 2);
	if (cnp->cn_flags & ISWHITEOUT) {
		ip->i_flags |= UF_OPAQUE;
		DIP_ASSIGN(ip, flags, ip->i_flags);
	}

	/*
	 * Bump link count in parent directory to reflect work done below.
	 */
	dp->i_nlink++;
	DIP_ASSIGN(dp, nlink, dp->i_nlink);
	dp->i_flag |= IN_CHANGE;
	if ((error = lfs_update(dvp, NULL, NULL, UPDATE_DIROP)) != 0)
		goto bad;

	/*
	 * Initialize directory with "." and "..". This used to use a
	 * static template but that adds moving parts for very little
	 * benefit.
	 */
	if ((error = lfs_balloc(tvp, (off_t)0, dirblksiz, cnp->cn_cred,
	    B_CLRBUF, &bp)) != 0)
		goto bad;
	ip->i_size = dirblksiz;
	DIP_ASSIGN(ip, size, dirblksiz);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	uvm_vnp_setsize(tvp, ip->i_size);
	dirp = bp->b_data;

	/* . */
	lfs_dir_setino(fs, dirp, ip->i_number);
	lfs_dir_setreclen(fs, dirp, LFS_DIRECTSIZ(fs, 1));
	lfs_dir_settype(fs, dirp, LFS_DT_DIR);
	lfs_dir_setnamlen(fs, dirp, 1);
	lfs_copydirname(fs, lfs_dir_nameptr(fs, dirp), ".", 1,
			LFS_DIRECTSIZ(fs, 1));
	dirp = LFS_NEXTDIR(fs, dirp);
	/* .. */
	lfs_dir_setino(fs, dirp, dp->i_number);
	lfs_dir_setreclen(fs, dirp, dirblksiz - LFS_DIRECTSIZ(fs, 1));
	lfs_dir_settype(fs, dirp, LFS_DT_DIR);
	lfs_dir_setnamlen(fs, dirp, 2);
	lfs_copydirname(fs, lfs_dir_nameptr(fs, dirp), "..", 2,
			dirblksiz - LFS_DIRECTSIZ(fs, 1));

	/*
	 * Directory set up; now install its entry in the parent directory.
	 */
	if ((error = VOP_BWRITE(bp->b_vp, bp)) != 0)
		goto bad;
	if ((error = lfs_update(tvp, NULL, NULL, UPDATE_DIROP)) != 0) {
		goto bad;
	}
	error = ulfs_direnter(dvp, ulr, tvp,
			      cnp, ip->i_number, LFS_IFTODT(ip->i_mode), bp);
 bad:
	if (error == 0) {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
		VOP_UNLOCK(tvp);
	} else {
		dp->i_nlink--;
		DIP_ASSIGN(dp, nlink, dp->i_nlink);
		dp->i_flag |= IN_CHANGE;
		/*
		 * No need to do an explicit lfs_truncate here, vrele will
		 * do this for us because we set the link count to 0.
		 */
		ip->i_nlink = 0;
		DIP_ASSIGN(ip, nlink, 0);
		ip->i_flag |= IN_CHANGE;
		/* If IN_ADIROP, account for it */
		lfs_unmark_vnode(tvp);
		vput(tvp);
	}

out:
	fstrans_done(dvp->v_mount);

	UNMARK_VNODE(dvp);
	UNMARK_VNODE(*vpp);
	if (error) {
		*vpp = NULL;
	}
	lfs_unset_dirop(fs, dvp, "mkdir");

	vrele(dvp);
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
	if ((error = lfs_set_dirop(dvp, vp)) != 0) {
		if (dvp == vp)
			vrele(vp);
		else
			vput(vp);
		vput(dvp);
		return error;
	}
	error = ulfs_remove(ap);
	if (ip->i_nlink == 0)
		lfs_orphan(ip->i_lfs, ip->i_number);

	UNMARK_VNODE(dvp);
	if (ap->a_vp) {
		UNMARK_VNODE(ap->a_vp);
	}
	lfs_unset_dirop(ip->i_lfs, dvp, "remove");
	vrele(dvp);
	if (ap->a_vp) {
		vrele(ap->a_vp);
	}

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
	if ((error = lfs_set_dirop(ap->a_dvp, ap->a_vp)) != 0) {
		if (ap->a_dvp == vp)
			vrele(ap->a_dvp);
		else
			vput(ap->a_dvp);
		vput(vp);
		return error;
	}
	error = ulfs_rmdir(ap);
	if (ip->i_nlink == 0)
		lfs_orphan(ip->i_lfs, ip->i_number);

	UNMARK_VNODE(ap->a_dvp);
	if (ap->a_vp) {
		UNMARK_VNODE(ap->a_vp);
	}
	lfs_unset_dirop(ip->i_lfs, ap->a_dvp, "rmdir");
	vrele(ap->a_dvp);
	if (ap->a_vp) {
		vrele(ap->a_vp);
	}

	return (error);
}

int
lfs_link(void *v)
{
	struct vop_link_v2_args	/* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct lfs *fs;
	struct vnode *dvp;
	int error;

	dvp = ap->a_dvp;

	fs = VFSTOULFS(dvp->v_mount)->um_lfs;
	ASSERT_NO_SEGLOCK(fs);
	if (fs->lfs_ronly) {
		return EROFS;
	}

	error = lfs_set_dirop(dvp, NULL);
	if (error) {
		return error;
	}

	error = ulfs_link(ap);

	UNMARK_VNODE(dvp);
	lfs_unset_dirop(fs, dvp, "link");
	vrele(dvp);

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

	fstrans_start(vp->v_mount, FSTRANS_SHARED);
	/*
	 * Copy from inode table
	 */
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode & ~LFS_IFMT;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	switch (vp->v_type) {
	    case VBLK:
	    case VCHR:
		vap->va_rdev = (dev_t)lfs_dino_getrdev(fs, ip->i_din);
		break;
	    default:
		vap->va_rdev = NODEV;
		break;
	}
	vap->va_size = vp->v_size;
	vap->va_atime.tv_sec = lfs_dino_getatime(fs, ip->i_din);
	vap->va_atime.tv_nsec = lfs_dino_getatimensec(fs, ip->i_din);
	vap->va_mtime.tv_sec = lfs_dino_getmtime(fs, ip->i_din);
	vap->va_mtime.tv_nsec = lfs_dino_getmtimensec(fs, ip->i_din);
	vap->va_ctime.tv_sec = lfs_dino_getctime(fs, ip->i_din);
	vap->va_ctime.tv_nsec = lfs_dino_getctimensec(fs, ip->i_din);
	vap->va_flags = ip->i_flags;
	vap->va_gen = ip->i_gen;
	/* this doesn't belong here */
	if (vp->v_type == VBLK)
		vap->va_blocksize = BLKDEV_IOSIZE;
	else if (vp->v_type == VCHR)
		vap->va_blocksize = MAXBSIZE;
	else
		vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_bytes = lfs_fsbtob(fs, ip->i_lfs_effnblks);
	vap->va_type = vp->v_type;
	vap->va_filerev = ip->i_modrev;
	fstrans_done(vp->v_mount);
	return (0);
}

/*
 * Check to make sure the inode blocks won't choke the buffer
 * cache, then call ulfs_setattr as usual.
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
	return ulfs_setattr(v);
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
		log(LOG_NOTICE, "%s: re-enabled log wrap\n",
		    lfs_sb_getfsmnt(fs));
		wakeup(&fs->lfs_wrappass);
		lfs_wakeup_cleaner(fs);
	}
	if (waitfor) {
		mtsleep(&fs->lfs_nextsegsleep, PCATCH | PUSER, "segment",
		    0, &lfs_lock);
	}

	return 0;
}

/*
 * Close called.
 *
 * Update the times on the inode.
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

	if ((ip->i_number == ULFS_ROOTINO || ip->i_number == LFS_IFILE_INUM) &&
	    fs->lfs_stoplwp == curlwp) {
		mutex_enter(&lfs_lock);
		log(LOG_NOTICE, "lfs_close: releasing log wrap control\n");
		lfs_wrapgo(fs, ip, 0);
		mutex_exit(&lfs_lock);
	}

	if (vp == ip->i_lfs->lfs_ivnode &&
	    vp->v_mount->mnt_iflag & IMNT_UNMOUNT)
		return 0;

	fstrans_start(vp->v_mount, FSTRANS_SHARED);
	if (vp->v_usecount > 1 && vp != ip->i_lfs->lfs_ivnode) {
		LFS_ITIMES(ip, NULL, NULL, NULL);
	}
	fstrans_done(vp->v_mount);
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
	 * or lock on the inode will be stalled.
	 */
	if (ip->i_nlink <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		lfs_vfree(vp, ip->i_number, ip->i_omode);

	mutex_enter(&lfs_lock);
	LFS_CLR_UINO(ip, IN_ALLMOD);
	mutex_exit(&lfs_lock);
	if ((error = ulfs_reclaim(vp)))
		return (error);

	/*
	 * Take us off the paging and/or dirop queues if we were on them.
	 * We shouldn't be on them.
	 */
	mutex_enter(&lfs_lock);
	if (ip->i_flags & IN_PAGING) {
		log(LOG_WARNING, "%s: reclaimed vnode is IN_PAGING\n",
		    lfs_sb_getfsmnt(fs));
		ip->i_flags &= ~IN_PAGING;
		TAILQ_REMOVE(&fs->lfs_pchainhd, ip, i_lfs_pchain);
	}
	if (vp->v_uflag & VU_DIROP) {
		panic("reclaimed vnode is VU_DIROP");
		vp->v_uflag &= ~VU_DIROP;
		TAILQ_REMOVE(&fs->lfs_dchainhd, ip, i_lfs_dchain);
	}
	mutex_exit(&lfs_lock);

	pool_put(&lfs_dinode_pool, ip->i_din);
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
 *
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 *
 * In order to avoid reading blocks that are in the process of being
 * written by the cleaner---and hence are not mutexed by the normal
 * buffer cache / page cache mechanisms---check for collisions before
 * reading.
 *
 * We inline ulfs_strategy to make sure that the VOP_BMAP occurs *before*
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
#define MAXLOOP 25
	int		i, sn, error, slept, loopcount;

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
	loopcount = 0;
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
		tbn = LFS_DBTOFSB(fs, bp->b_blkno);
		sn = lfs_dtosn(fs, tbn);
		slept = 0;
		for (i = 0; i < fs->lfs_cleanind; i++) {
			if (sn == lfs_dtosn(fs, fs->lfs_cleanint[i]) &&
			    tbn >= fs->lfs_cleanint[i]) {
				DLOG((DLOG_CLEAN,
				      "lfs_strategy: ino %d lbn %" PRId64
				      " ind %d sn %d fsb %" PRIx64
				      " given sn %d fsb %" PRIx64 "\n",
				      ip->i_number, bp->b_lblkno, i,
				      lfs_dtosn(fs, fs->lfs_cleanint[i]),
				      fs->lfs_cleanint[i], sn, tbn));
				DLOG((DLOG_CLEAN,
				      "lfs_strategy: sleeping on ino %d lbn %"
				      PRId64 "\n", ip->i_number, bp->b_lblkno));
				mutex_enter(&lfs_lock);
				if (LFS_SEGLOCK_HELD(fs) && fs->lfs_iocount) {
					/*
					 * Cleaner can't wait for itself.
					 * Instead, wait for the blocks
					 * to be written to disk.
					 * XXX we need pribio in the test
					 * XXX here.
					 */
 					mtsleep(&fs->lfs_iocount,
 						(PRIBIO + 1) | PNORELOCK,
						"clean2", hz/10 + 1,
 						&lfs_lock);
					slept = 1;
					++loopcount;
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
		if (loopcount > MAXLOOP) {
			printf("lfs_strategy: breaking out of clean2 loop\n");
			break;
		}
	}
	mutex_exit(&lfs_lock);

	vp = ip->i_devvp;
	return VOP_STRATEGY(vp, bp);
}

/*
 * Inline lfs_segwrite/lfs_writevnodes, but just for dirops.
 * Technically this is a checkpoint (the on-disk state is valid)
 * even though we are leaving out all the file data.
 */
int
lfs_flush_dirops(struct lfs *fs)
{
	struct inode *ip, *nip;
	struct vnode *vp;
	extern int lfs_dostats; /* XXX this does not belong here */
	struct segment *sp;
	SEGSUM *ssp;
	int flags = 0;
	int error = 0;

	ASSERT_MAYBE_SEGLOCK(fs);
	KASSERT(fs->lfs_nadirop == 0);

	if (fs->lfs_ronly)
		return EROFS;

	mutex_enter(&lfs_lock);
	if (TAILQ_FIRST(&fs->lfs_dchainhd) == NULL) {
		mutex_exit(&lfs_lock);
		return 0;
	} else
		mutex_exit(&lfs_lock);

	if (lfs_dostats)
		++lfs_stats.flush_invoked;

	lfs_imtime(fs);
	lfs_seglock(fs, flags);
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
		mutex_enter(vp->v_interlock);

		KASSERT((ip->i_flag & IN_ADIROP) == 0);
		KASSERT(vp->v_uflag & VU_DIROP);
		KASSERT(vdead_check(vp, VDEAD_NOWAIT) == 0);

		/*
		 * All writes to directories come from dirops; all
		 * writes to files' direct blocks go through the page
		 * cache, which we're not touching.  Reads to files
		 * and/or directories will not be affected by writing
		 * directory blocks inodes and file inodes.  So we don't
		 * really need to lock.
		 */
		if (vdead_check(vp, VDEAD_NOWAIT) != 0) {
			mutex_exit(vp->v_interlock);
			mutex_enter(&lfs_lock);
			continue;
		}
		mutex_exit(vp->v_interlock);
		/* XXX see below
		 * waslocked = VOP_ISLOCKED(vp);
		 */
		if (vp->v_type != VREG &&
		    ((ip->i_flag & IN_ALLMOD) || !VPISEMPTY(vp))) {
			error = lfs_writefile(fs, sp, vp);
			if (!VPISEMPTY(vp) && !WRITEINPROG(vp) &&
			    !(ip->i_flag & IN_ALLMOD)) {
			    	mutex_enter(&lfs_lock);
				LFS_SET_UINO(ip, IN_MODIFIED);
			    	mutex_exit(&lfs_lock);
			}
			if (error && (sp->seg_flags & SEGM_SINGLE)) {
				mutex_enter(&lfs_lock);
				error = EAGAIN;
				break;
			}
		}
		KDASSERT(ip->i_number != LFS_IFILE_INUM);
		error = lfs_writeinode(fs, sp, ip);
		mutex_enter(&lfs_lock);
		if (error && (sp->seg_flags & SEGM_SINGLE)) {
			error = EAGAIN;
			break;
		}

		/*
		 * We might need to update these inodes again,
		 * for example, if they have data blocks to write.
		 * Make sure that after this flush, they are still
		 * marked IN_MODIFIED so that we don't forget to
		 * write them.
		 */
		/* XXX only for non-directories? --KS */
		LFS_SET_UINO(ip, IN_MODIFIED);
	}
	mutex_exit(&lfs_lock);
	/* We've written all the dirops there are */
	ssp = (SEGSUM *)sp->segsum;
	lfs_ss_setflags(fs, ssp, lfs_ss_getflags(fs, ssp) & ~(SS_CONT));
	lfs_finalize_fs_seguse(fs);
	(void) lfs_writeseg(fs, sp);
	lfs_segunlock(fs);

	return error;
}

/*
 * Flush all vnodes for which the pagedaemon has requested pageouts.
 * Skip over any files that are marked VU_DIROP (since lfs_flush_dirop()
 * has just run, this would be an error).  If we have to skip a vnode
 * for any reason, just skip it; if we have to wait for the cleaner,
 * abort.  The writer daemon will call us again later.
 */
int
lfs_flush_pchain(struct lfs *fs)
{
	struct inode *ip, *nip;
	struct vnode *vp;
	extern int lfs_dostats;
	struct segment *sp;
	int error, error2;

	ASSERT_NO_SEGLOCK(fs);

	if (fs->lfs_ronly)
		return EROFS;

	mutex_enter(&lfs_lock);
	if (TAILQ_FIRST(&fs->lfs_pchainhd) == NULL) {
		mutex_exit(&lfs_lock);
		return 0;
	} else
		mutex_exit(&lfs_lock);

	/* Get dirops out of the way */
	if ((error = lfs_flush_dirops(fs)) != 0)
		return error;

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
		struct mount *mp = ITOV(ip)->v_mount;
		ino_t ino = ip->i_number;

		nip = TAILQ_NEXT(ip, i_lfs_pchain);

		if (!(ip->i_flags & IN_PAGING))
			goto top;

		mutex_exit(&lfs_lock);
		if (vcache_get(mp, &ino, sizeof(ino), &vp) != 0) {
			mutex_enter(&lfs_lock);
			continue;
		};
		if (vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
			vrele(vp);
			mutex_enter(&lfs_lock);
			continue;
		}
		ip = VTOI(vp);
		mutex_enter(&lfs_lock);
		if ((vp->v_uflag & VU_DIROP) != 0 || vp->v_type != VREG ||
		    !(ip->i_flags & IN_PAGING)) {
			mutex_exit(&lfs_lock);
			vput(vp);
			mutex_enter(&lfs_lock);
			goto top;
		}
		mutex_exit(&lfs_lock);

		error = lfs_writefile(fs, sp, vp);
		if (!VPISEMPTY(vp) && !WRITEINPROG(vp) &&
		    !(ip->i_flag & IN_ALLMOD)) {
		    	mutex_enter(&lfs_lock);
			LFS_SET_UINO(ip, IN_MODIFIED);
		    	mutex_exit(&lfs_lock);
		}
		KDASSERT(ip->i_number != LFS_IFILE_INUM);
		error2 = lfs_writeinode(fs, sp, ip);

		VOP_UNLOCK(vp);
		vrele(vp);

		if (error == EAGAIN || error2 == EAGAIN) {
			lfs_writeseg(fs, sp);
			mutex_enter(&lfs_lock);
			break;
		}
		mutex_enter(&lfs_lock);
	}
	mutex_exit(&lfs_lock);
	(void) lfs_writeseg(fs, sp);
	lfs_segunlock(fs);

	return 0;
}

/*
 * Conversion for compat.
 */
static void
block_info_from_70(BLOCK_INFO *bi, const BLOCK_INFO_70 *bi70)
{
	bi->bi_inode = bi70->bi_inode;
	bi->bi_lbn = bi70->bi_lbn;
	bi->bi_daddr = bi70->bi_daddr;
	bi->bi_segcreate = bi70->bi_segcreate;
	bi->bi_version = bi70->bi_version;
	bi->bi_bp = bi70->bi_bp;
	bi->bi_size = bi70->bi_size;
}

static void
block_info_to_70(BLOCK_INFO_70 *bi70, const BLOCK_INFO *bi)
{
	bi70->bi_inode = bi->bi_inode;
	bi70->bi_lbn = bi->bi_lbn;
	bi70->bi_daddr = bi->bi_daddr;
	bi70->bi_segcreate = bi->bi_segcreate;
	bi70->bi_version = bi->bi_version;
	bi70->bi_bp = bi->bi_bp;
	bi70->bi_size = bi->bi_size;
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
	BLOCK_INFO_70 *blkiov70;
	CLEANERINFO *cip;
	SEGUSE *sup;
	int blkcnt, i, error;
	size_t fh_size;
	struct lfs_fcntl_markv blkvp;
	struct lfs_fcntl_markv_70 blkvp70;
	struct lwp *l;
	fsid_t *fsidp;
	struct lfs *fs;
	struct buf *bp;
	fhandle_t *fhp;
	daddr_t off;
	int oclean;

	/* Only respect LFS fcntls on fs root or Ifile */
	if (VTOI(ap->a_vp)->i_number != ULFS_ROOTINO &&
	    VTOI(ap->a_vp)->i_number != LFS_IFILE_INUM) {
		return ulfs_fcntl(v);
	}

	/* Avoid locking a draining lock */
	if (ap->a_vp->v_mount->mnt_iflag & IMNT_UNMOUNT) {
		return ESHUTDOWN;
	}

	/* LFS control and monitoring fcntls are available only to root */
	l = curlwp;
	if (((ap->a_command & 0xff00) >> 8) == 'L' &&
	    (error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_LFS,
	     KAUTH_REQ_SYSTEM_LFS_FCNTL, NULL, NULL, NULL)) != 0)
		return (error);

	fs = VTOI(ap->a_vp)->i_lfs;
	fsidp = &ap->a_vp->v_mount->mnt_stat.f_fsidx;

	error = 0;
	switch ((int)ap->a_command) {
	    case LFCNSEGWAITALL_COMPAT_50:
	    case LFCNSEGWAITALL_COMPAT:
		fsidp = NULL;
		/* FALLTHROUGH */
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
		/* FALLTHROUGH */
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

	    case LFCNBMAPV_COMPAT_70:
	    case LFCNMARKV_COMPAT_70:
		blkvp70 = *(struct lfs_fcntl_markv_70 *)ap->a_data;

		blkcnt = blkvp70.blkcnt;
		if ((u_int) blkcnt > LFS_MARKV_MAXBLKCNT)
			return (EINVAL);
		blkiov = lfs_malloc(fs, blkcnt * sizeof(BLOCK_INFO), LFS_NB_BLKIOV);
		blkiov70 = lfs_malloc(fs, sizeof(BLOCK_INFO_70), LFS_NB_BLKIOV);
		for (i = 0; i < blkcnt; i++) {
			error = copyin(&blkvp70.blkiov[i], blkiov70,
				       sizeof(*blkiov70));
			if (error) {
				lfs_free(fs, blkiov70, LFS_NB_BLKIOV);
				lfs_free(fs, blkiov, LFS_NB_BLKIOV);
				return error;
			}
			block_info_from_70(&blkiov[i], blkiov70);
		}

		mutex_enter(&lfs_lock);
		++fs->lfs_sleepers;
		mutex_exit(&lfs_lock);
		if (ap->a_command == LFCNBMAPV)
			error = lfs_bmapv(l, fsidp, blkiov, blkcnt);
		else /* LFCNMARKV */
			error = lfs_markv(l, fsidp, blkiov, blkcnt);
		if (error == 0) {
			for (i = 0; i < blkcnt; i++) {
				block_info_to_70(blkiov70, &blkiov[i]);
				error = copyout(blkiov70, &blkvp70.blkiov[i],
						sizeof(*blkiov70));
				if (error) {
					break;
				}
			}
		}
		mutex_enter(&lfs_lock);
		if (--fs->lfs_sleepers == 0)
			wakeup(&fs->lfs_sleepers);
		mutex_exit(&lfs_lock);
		lfs_free(fs, blkiov, LFS_NB_BLKIOV);
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
			error = lfs_bmapv(l, fsidp, blkiov, blkcnt);
		else /* LFCNMARKV */
			error = lfs_markv(l, fsidp, blkiov, blkcnt);
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
		off = lfs_sb_getoffset(fs);
		lfs_seglock(fs, SEGM_FORCE_CKP | SEGM_CKP);
		lfs_flush_dirops(fs);
		LFS_CLEANERINFO(cip, fs, bp);
		oclean = lfs_ci_getclean(fs, cip);
		LFS_SYNC_CLEANERINFO(cip, fs, bp, 1);
		lfs_segwrite(ap->a_vp->v_mount, SEGM_FORCE_CKP);
		fs->lfs_sp->seg_flags |= SEGM_PROT;
		lfs_segunlock(fs);
		lfs_writer_leave(fs);

#ifdef DEBUG
		LFS_CLEANERINFO(cip, fs, bp);
		DLOG((DLOG_CLEAN, "lfs_fcntl: reclaim wrote %" PRId64
		      " blocks, cleaned %" PRId32 " segments (activesb %d)\n",
		      lfs_sb_getoffset(fs) - off,
		      lfs_ci_getclean(fs, cip) - oclean,
		      fs->lfs_activesb));
		LFS_SYNC_CLEANERINFO(cip, fs, bp, 0);
#else
		__USE(oclean);
		__USE(off);
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
			log(LOG_NOTICE, "%s: disabled log wrap\n",
			    lfs_sb_getfsmnt(fs));
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
			vref(ap->a_vp);
			VTOI(ap->a_vp)->i_lfs_iflags |= LFSI_WRAPWAIT;
			log(LOG_NOTICE, "LFCNPASS waiting for log wrap\n");
			error = mtsleep(&fs->lfs_nowrap, PCATCH | PUSER,
				"segwrap", 0, &lfs_lock);
			log(LOG_NOTICE, "LFCNPASS done waiting\n");
			VTOI(ap->a_vp)->i_lfs_iflags &= ~LFSI_WRAPWAIT;
			vrele(ap->a_vp);
		}
		mutex_exit(&lfs_lock);
		return error;

	    case LFCNWRAPSTATUS:
		mutex_enter(&lfs_lock);
		*(int *)ap->a_data = fs->lfs_wrapstatus;
		mutex_exit(&lfs_lock);
		return 0;

	    default:
		return ulfs_fcntl(v);
	}
	return 0;
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

	olbn = lfs_lblkno(fs, ip->i_size);
	nlbn = lfs_lblkno(fs, size);
	if (!(flags & GOP_SIZE_MEM) && nlbn < ULFS_NDADDR && olbn <= nlbn) {
		*eobp = lfs_fragroundup(fs, size);
	} else {
		*eobp = lfs_blkroundup(fs, size);
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

	struct inode *ip = VTOI(ap->a_vp);
	struct lfs *fs = ip->i_lfs;

#ifdef DDB
	vfs_vnode_print(ap->a_vp, 0, printf);
#endif
	lfs_dump_dinode(fs, ip->i_din);
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
	return ulfs_mmap(v);
}

static int
lfs_openextattr(void *v)
{
	struct vop_openextattr_args /* {
		struct vnode *a_vp;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	struct ulfsmount *ump = ip->i_ump;
	//struct lfs *fs = ip->i_lfs;

	/* Not supported for ULFS1 file systems. */
	if (ump->um_fstype == ULFS1)
		return (EOPNOTSUPP);

	/* XXX Not implemented for ULFS2 file systems. */
	return (EOPNOTSUPP);
}

static int
lfs_closeextattr(void *v)
{
	struct vop_closeextattr_args /* {
		struct vnode *a_vp;
		int a_commit;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	struct ulfsmount *ump = ip->i_ump;
	//struct lfs *fs = ip->i_lfs;

	/* Not supported for ULFS1 file systems. */
	if (ump->um_fstype == ULFS1)
		return (EOPNOTSUPP);

	/* XXX Not implemented for ULFS2 file systems. */
	return (EOPNOTSUPP);
}

static int
lfs_getextattr(void *v)
{
	struct vop_getextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ulfsmount *ump = ip->i_ump;
	//struct lfs *fs = ip->i_lfs;
	int error;

	if (ump->um_fstype == ULFS1) {
#ifdef LFS_EXTATTR
		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ulfs_getextattr(ap);
		fstrans_done(vp->v_mount);
#else
		error = EOPNOTSUPP;
#endif
		return error;
	}

	/* XXX Not implemented for ULFS2 file systems. */
	return (EOPNOTSUPP);
}

static int
lfs_setextattr(void *v)
{
	struct vop_setextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		const char *a_name;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ulfsmount *ump = ip->i_ump;
	//struct lfs *fs = ip->i_lfs;
	int error;

	if (ump->um_fstype == ULFS1) {
#ifdef LFS_EXTATTR
		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ulfs_setextattr(ap);
		fstrans_done(vp->v_mount);
#else
		error = EOPNOTSUPP;
#endif
		return error;
	}

	/* XXX Not implemented for ULFS2 file systems. */
	return (EOPNOTSUPP);
}

static int
lfs_listextattr(void *v)
{
	struct vop_listextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		struct uio *a_uio;
		size_t *a_size;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ulfsmount *ump = ip->i_ump;
	//struct lfs *fs = ip->i_lfs;
	int error;

	if (ump->um_fstype == ULFS1) {
#ifdef LFS_EXTATTR
		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ulfs_listextattr(ap);
		fstrans_done(vp->v_mount);
#else
		error = EOPNOTSUPP;
#endif
		return error;
	}

	/* XXX Not implemented for ULFS2 file systems. */
	return (EOPNOTSUPP);
}

static int
lfs_deleteextattr(void *v)
{
	struct vop_deleteextattr_args /* {
		struct vnode *a_vp;
		int a_attrnamespace;
		kauth_cred_t a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	struct ulfsmount *ump = ip->i_ump;
	//struct fs *fs = ip->i_lfs;
	int error;

	if (ump->um_fstype == ULFS1) {
#ifdef LFS_EXTATTR
		fstrans_start(vp->v_mount, FSTRANS_SHARED);
		error = ulfs_deleteextattr(ap);
		fstrans_done(vp->v_mount);
#else
		error = EOPNOTSUPP;
#endif
		return error;
	}

	/* XXX Not implemented for ULFS2 file systems. */
	return (EOPNOTSUPP);
}
