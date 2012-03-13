/*	$NetBSD: lfs_vfsops.c,v 1.291 2011/11/14 18:35:14 hannken Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003, 2007, 2007
 *     The NetBSD Foundation, Inc.
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
/*-
 * Copyright (c) 1989, 1991, 1993, 1994
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
 *	@(#)lfs_vfsops.c	8.20 (Berkeley) 6/10/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_vfsops.c,v 1.291 2011/11/14 18:35:14 hannken Exp $");

#if defined(_KERNEL_OPT)
#include "opt_lfs.h"
#include "opt_quota.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kthread.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/module.h>
#include <sys/syscallvar.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include <miscfs/specfs/specdev.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <uvm/uvm.h>
#include <uvm/uvm_stat.h>
#include <uvm/uvm_pager.h>
#include <uvm/uvm_pdaemon.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_extern.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>

MODULE(MODULE_CLASS_VFS, lfs, "ffs");

static int lfs_gop_write(struct vnode *, struct vm_page **, int, int);
static bool lfs_issequential_hole(const struct ufsmount *,
    daddr_t, daddr_t);

static int lfs_mountfs(struct vnode *, struct mount *, struct lwp *);

static struct sysctllog *lfs_sysctl_log;

extern const struct vnodeopv_desc lfs_vnodeop_opv_desc;
extern const struct vnodeopv_desc lfs_specop_opv_desc;
extern const struct vnodeopv_desc lfs_fifoop_opv_desc;

pid_t lfs_writer_daemon = 0;
int lfs_do_flush = 0;
#ifdef LFS_KERNEL_RFW
int lfs_do_rfw = 0;
#endif

const struct vnodeopv_desc * const lfs_vnodeopv_descs[] = {
	&lfs_vnodeop_opv_desc,
	&lfs_specop_opv_desc,
	&lfs_fifoop_opv_desc,
	NULL,
};

struct vfsops lfs_vfsops = {
	MOUNT_LFS,
	sizeof (struct ufs_args),
	lfs_mount,
	ufs_start,
	lfs_unmount,
	ufs_root,
	ufs_quotactl,
	lfs_statvfs,
	lfs_sync,
	lfs_vget,
	lfs_fhtovp,
	lfs_vptofh,
	lfs_init,
	lfs_reinit,
	lfs_done,
	lfs_mountroot,
	(int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
	vfs_stdextattrctl,
	(void *)eopnotsupp,	/* vfs_suspendctl */
	genfs_renamelock_enter,
	genfs_renamelock_exit,
	(void *)eopnotsupp,
	lfs_vnodeopv_descs,
	0,
	{ NULL, NULL },
};

const struct genfs_ops lfs_genfsops = {
	.gop_size = lfs_gop_size,
	.gop_alloc = ufs_gop_alloc,
	.gop_write = lfs_gop_write,
	.gop_markupdate = ufs_gop_markupdate,
};

static const struct ufs_ops lfs_ufsops = {
	.uo_itimes = NULL,
	.uo_update = lfs_update,
	.uo_truncate = lfs_truncate,
	.uo_valloc = lfs_valloc,
	.uo_vfree = lfs_vfree,
	.uo_balloc = lfs_balloc,
	.uo_unmark_vnode = lfs_unmark_vnode,
};

struct shortlong {
	const char *sname;
	const char *lname;
};

static int
sysctl_lfs_dostats(SYSCTLFN_ARGS)
{
	extern struct lfs_stats lfs_stats;
	extern int lfs_dostats;
	int error;

	error = sysctl_lookup(SYSCTLFN_CALL(rnode));
	if (error || newp == NULL)
		return (error);

	if (lfs_dostats == 0)
		memset(&lfs_stats, 0, sizeof(lfs_stats));

	return (0);
}

static void
lfs_sysctl_setup(struct sysctllog **clog)
{
	int i;
	extern int lfs_writeindir, lfs_dostats, lfs_clean_vnhead,
		   lfs_fs_pagetrip, lfs_ignore_lazy_sync;
#ifdef DEBUG
	extern int lfs_debug_log_subsys[DLOG_MAX];
	struct shortlong dlog_names[DLOG_MAX] = { /* Must match lfs.h ! */
		{ "rollforward", "Debug roll-forward code" },
		{ "alloc",	"Debug inode allocation and free list" },
		{ "avail",	"Debug space-available-now accounting" },
		{ "flush",	"Debug flush triggers" },
		{ "lockedlist",	"Debug locked list accounting" },
		{ "vnode_verbose", "Verbose per-vnode-written debugging" },
		{ "vnode",	"Debug vnode use during segment write" },
		{ "segment",	"Debug segment writing" },
		{ "seguse",	"Debug segment used-bytes accounting" },
		{ "cleaner",	"Debug cleaning routines" },
		{ "mount",	"Debug mount/unmount routines" },
		{ "pagecache",	"Debug UBC interactions" },
		{ "dirop",	"Debug directory-operation accounting" },
		{ "malloc",	"Debug private malloc accounting" },
	};
#endif /* DEBUG */
	struct shortlong stat_names[] = { /* Must match lfs.h! */
		{ "segsused",	    "Number of new segments allocated" },
		{ "psegwrites",	    "Number of partial-segment writes" },
		{ "psyncwrites",    "Number of synchronous partial-segment"
				    " writes" },
		{ "pcleanwrites",   "Number of partial-segment writes by the"
				    " cleaner" },
		{ "blocktot",       "Number of blocks written" },
		{ "cleanblocks",    "Number of blocks written by the cleaner" },
		{ "ncheckpoints",   "Number of checkpoints made" },
		{ "nwrites",        "Number of whole writes" },
		{ "nsync_writes",   "Number of synchronous writes" },
		{ "wait_exceeded",  "Number of times writer waited for"
				    " cleaner" },
		{ "write_exceeded", "Number of times writer invoked flush" },
		{ "flush_invoked",  "Number of times flush was invoked" },
		{ "vflush_invoked", "Number of time vflush was called" },
		{ "clean_inlocked", "Number of vnodes skipped for VI_XLOCK" },
		{ "clean_vnlocked", "Number of vnodes skipped for vget failure" },
		{ "segs_reclaimed", "Number of segments reclaimed" },
	};

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "vfs", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "lfs",
		       SYSCTL_DESCR("Log-structured file system"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, 5, CTL_EOL);
	/*
	 * XXX the "5" above could be dynamic, thereby eliminating one
	 * more instance of the "number to vfs" mapping problem, but
	 * "5" is the order as taken from sys/mount.h
	 */

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "flushindir", NULL,
		       NULL, 0, &lfs_writeindir, 0,
		       CTL_VFS, 5, LFS_WRITEINDIR, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "clean_vnhead", NULL,
		       NULL, 0, &lfs_clean_vnhead, 0,
		       CTL_VFS, 5, LFS_CLEAN_VNHEAD, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "dostats",
		       SYSCTL_DESCR("Maintain statistics on LFS operations"),
		       sysctl_lfs_dostats, 0, &lfs_dostats, 0,
		       CTL_VFS, 5, LFS_DOSTATS, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "pagetrip",
		       SYSCTL_DESCR("How many dirty pages in fs triggers"
				    " a flush"),
		       NULL, 0, &lfs_fs_pagetrip, 0,
		       CTL_VFS, 5, LFS_FS_PAGETRIP, CTL_EOL);
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "ignore_lazy_sync",
		       SYSCTL_DESCR("Lazy Sync is ignored entirely"),
		       NULL, 0, &lfs_ignore_lazy_sync, 0,
		       CTL_VFS, 5, LFS_IGNORE_LAZY_SYNC, CTL_EOL);
#ifdef LFS_KERNEL_RFW
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "rfw",
		       SYSCTL_DESCR("Use in-kernel roll-forward on mount"),
		       NULL, 0, &lfs_do_rfw, 0,
		       CTL_VFS, 5, LFS_DO_RFW, CTL_EOL);
#endif

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "stats",
		       SYSCTL_DESCR("Debugging options"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, 5, LFS_STATS, CTL_EOL);
	for (i = 0; i < sizeof(struct lfs_stats) / sizeof(u_int); i++) {
		sysctl_createv(clog, 0, NULL, NULL,
			       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
			       CTLTYPE_INT, stat_names[i].sname,
			       SYSCTL_DESCR(stat_names[i].lname),
			       NULL, 0, &(((u_int *)&lfs_stats.segsused)[i]),
			       0, CTL_VFS, 5, LFS_STATS, i, CTL_EOL);
	}

#ifdef DEBUG
	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "debug",
		       SYSCTL_DESCR("Debugging options"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, 5, LFS_DEBUGLOG, CTL_EOL);
	for (i = 0; i < DLOG_MAX; i++) {
		sysctl_createv(clog, 0, NULL, NULL,
			       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			       CTLTYPE_INT, dlog_names[i].sname,
			       SYSCTL_DESCR(dlog_names[i].lname),
			       NULL, 0, &(lfs_debug_log_subsys[i]), 0,
			       CTL_VFS, 5, LFS_DEBUGLOG, i, CTL_EOL);
	}
#endif
}

/* old cleaner syscall interface.  see VOP_FCNTL() */
static const struct syscall_package lfs_syscalls[] = {
	{ SYS_lfs_bmapv,	0, (sy_call_t *)sys_lfs_bmapv		},
	{ SYS_lfs_markv,	0, (sy_call_t *)sys_lfs_markv		},
	{ SYS_lfs_segclean,	0, (sy_call_t *)sys___lfs_segwait50	},
	{ 0, 0, NULL },
};

static int
lfs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = syscall_establish(NULL, lfs_syscalls);
		if (error)
			return error;
		error = vfs_attach(&lfs_vfsops);
		if (error != 0) {
			syscall_disestablish(NULL, lfs_syscalls);
			break;
		}
		lfs_sysctl_setup(&lfs_sysctl_log);
		break;
	case MODULE_CMD_FINI:
		error = vfs_detach(&lfs_vfsops);
		if (error != 0)
			break;
		syscall_disestablish(NULL, lfs_syscalls);
		sysctl_teardown(&lfs_sysctl_log);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

/*
 * XXX Same structure as FFS inodes?  Should we share a common pool?
 */
struct pool lfs_inode_pool;
struct pool lfs_dinode_pool;
struct pool lfs_inoext_pool;
struct pool lfs_lbnentry_pool;

/*
 * The writer daemon.  UVM keeps track of how many dirty pages we are holding
 * in lfs_subsys_pages; the daemon flushes the filesystem when this value
 * crosses the (user-defined) threshhold LFS_MAX_PAGES.
 */
static void
lfs_writerd(void *arg)
{
	struct mount *mp, *nmp;
	struct lfs *fs;
	int fsflags;
	int loopcount;

	lfs_writer_daemon = curproc->p_pid;

	mutex_enter(&lfs_lock);
	for (;;) {
		mtsleep(&lfs_writer_daemon, PVM | PNORELOCK, "lfswriter", hz/10,
		    &lfs_lock);

		/*
		 * Look through the list of LFSs to see if any of them
		 * have requested pageouts.
		 */
		mutex_enter(&mountlist_lock);
		for (mp = CIRCLEQ_FIRST(&mountlist); mp != (void *)&mountlist;
		     mp = nmp) {
			if (vfs_busy(mp, &nmp)) {
				continue;
			}
			if (strncmp(mp->mnt_stat.f_fstypename, MOUNT_LFS,
			    sizeof(mp->mnt_stat.f_fstypename)) == 0) {
				fs = VFSTOUFS(mp)->um_lfs;
				mutex_enter(&lfs_lock);
				fsflags = 0;
				if ((fs->lfs_dirvcount > LFS_MAX_FSDIROP(fs) ||
				     lfs_dirvcount > LFS_MAX_DIROP) &&
				    fs->lfs_dirops == 0)
					fsflags |= SEGM_CKP;
				if (fs->lfs_pdflush) {
					DLOG((DLOG_FLUSH, "lfs_writerd: pdflush set\n"));
					fs->lfs_pdflush = 0;
					lfs_flush_fs(fs, fsflags);
					mutex_exit(&lfs_lock);
				} else if (!TAILQ_EMPTY(&fs->lfs_pchainhd)) {
					DLOG((DLOG_FLUSH, "lfs_writerd: pchain non-empty\n"));
					mutex_exit(&lfs_lock);
					lfs_writer_enter(fs, "wrdirop");
					lfs_flush_pchain(fs);
					lfs_writer_leave(fs);
				} else
					mutex_exit(&lfs_lock);
			}
			vfs_unbusy(mp, false, &nmp);
		}
		mutex_exit(&mountlist_lock);

		/*
		 * If global state wants a flush, flush everything.
		 */
		mutex_enter(&lfs_lock);
		loopcount = 0;
		if (lfs_do_flush || locked_queue_count > LFS_MAX_BUFS ||
			locked_queue_bytes > LFS_MAX_BYTES ||
			lfs_subsys_pages > LFS_MAX_PAGES) {

			if (lfs_do_flush) {
				DLOG((DLOG_FLUSH, "daemon: lfs_do_flush\n"));
			}
			if (locked_queue_count > LFS_MAX_BUFS) {
				DLOG((DLOG_FLUSH, "daemon: lqc = %d, max %d\n",
				      locked_queue_count, LFS_MAX_BUFS));
			}
			if (locked_queue_bytes > LFS_MAX_BYTES) {
				DLOG((DLOG_FLUSH, "daemon: lqb = %ld, max %ld\n",
				      locked_queue_bytes, LFS_MAX_BYTES));
			}
			if (lfs_subsys_pages > LFS_MAX_PAGES) {
				DLOG((DLOG_FLUSH, "daemon: lssp = %d, max %d\n",
				      lfs_subsys_pages, LFS_MAX_PAGES));
			}

			lfs_flush(NULL, SEGM_WRITERD, 0);
			lfs_do_flush = 0;
		}
	}
	/* NOTREACHED */
}

/*
 * Initialize the filesystem, most work done by ufs_init.
 */
void
lfs_init(void)
{

	malloc_type_attach(M_SEGMENT);
	pool_init(&lfs_inode_pool, sizeof(struct inode), 0, 0, 0,
	    "lfsinopl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&lfs_dinode_pool, sizeof(struct ufs1_dinode), 0, 0, 0,
	    "lfsdinopl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&lfs_inoext_pool, sizeof(struct lfs_inode_ext), 8, 0, 0,
	    "lfsinoextpl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&lfs_lbnentry_pool, sizeof(struct lbnentry), 0, 0, 0,
	    "lfslbnpool", &pool_allocator_nointr, IPL_NONE);
	ufs_init();

#ifdef DEBUG
	memset(lfs_log, 0, sizeof(lfs_log));
#endif
	mutex_init(&lfs_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&locked_queue_cv, "lfsbuf");
	cv_init(&lfs_writing_cv, "lfsflush");
}

void
lfs_reinit(void)
{
	ufs_reinit();
}

void
lfs_done(void)
{
	ufs_done();
	mutex_destroy(&lfs_lock);
	cv_destroy(&locked_queue_cv);
	cv_destroy(&lfs_writing_cv);
	pool_destroy(&lfs_inode_pool);
	pool_destroy(&lfs_dinode_pool);
	pool_destroy(&lfs_inoext_pool);
	pool_destroy(&lfs_lbnentry_pool);
	malloc_type_detach(M_SEGMENT);
}

/*
 * Called by main() when ufs is going to be mounted as root.
 */
int
lfs_mountroot(void)
{
	extern struct vnode *rootvp;
	struct lfs *fs = NULL;				/* LFS */
	struct mount *mp;
	struct lwp *l = curlwp;
	struct ufsmount *ump;
	int error;

	if (device_class(root_device) != DV_DISK)
		return (ENODEV);

	if (rootdev == NODEV)
		return (ENODEV);
	if ((error = vfs_rootmountalloc(MOUNT_LFS, "root_device", &mp))) {
		vrele(rootvp);
		return (error);
	}
	if ((error = lfs_mountfs(rootvp, mp, l))) {
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		return (error);
	}
	mutex_enter(&mountlist_lock);
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mutex_exit(&mountlist_lock);
	ump = VFSTOUFS(mp);
	fs = ump->um_lfs;
	memset(fs->lfs_fsmnt, 0, sizeof(fs->lfs_fsmnt));
	(void)copystr(mp->mnt_stat.f_mntonname, fs->lfs_fsmnt, MNAMELEN - 1, 0);
	(void)lfs_statvfs(mp, &mp->mnt_stat);
	vfs_unbusy(mp, false, NULL);
	setrootfstime((time_t)(VFSTOUFS(mp)->um_lfs->lfs_tstamp));
	return (0);
}

/*
 * VFS Operations.
 *
 * mount system call
 */
int
lfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	struct vnode *devvp;
	struct ufs_args *args = data;
	struct ufsmount *ump = NULL;
	struct lfs *fs = NULL;				/* LFS */
	int error = 0, update;
	mode_t accessmode;

	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {
		ump = VFSTOUFS(mp);
		if (ump == NULL)
			return EIO;
		args->fspec = NULL;
		*data_len = sizeof *args;
		return 0;
	}

	update = mp->mnt_flag & MNT_UPDATE;

	/* Check arguments */
	if (args->fspec != NULL) {
		/*
		 * Look up the name and verify that it's sane.
		 */
		error = namei_simple_user(args->fspec,
					NSM_FOLLOW_NOEMULROOT, &devvp);
		if (error != 0)
			return (error);

		if (!update) {
			/*
			 * Be sure this is a valid block device
			 */
			if (devvp->v_type != VBLK)
				error = ENOTBLK;
			else if (bdevsw_lookup(devvp->v_rdev) == NULL)
				error = ENXIO;
		} else {
			/*
			 * Be sure we're still naming the same device
			 * used for our initial mount
			 */
			ump = VFSTOUFS(mp);
			if (devvp != ump->um_devvp) {
				if (devvp->v_rdev != ump->um_devvp->v_rdev)
					error = EINVAL;
				else {
					vrele(devvp);
					devvp = ump->um_devvp;
					vref(devvp);
				}
			}
		}
	} else {
		if (!update) {
			/* New mounts must have a filename for the device */
			return (EINVAL);
		} else {
			/* Use the extant mount */
			ump = VFSTOUFS(mp);
			devvp = ump->um_devvp;
			vref(devvp);
		}
	}


	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (error == 0) {
		accessmode = VREAD;
		if (update ?
		    (mp->mnt_iflag & IMNT_WANTRDWR) != 0 :
		    (mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = genfs_can_mount(devvp, accessmode, l->l_cred);
		VOP_UNLOCK(devvp);
	}

	if (error) {
		vrele(devvp);
		return (error);
	}

	if (!update) {
		int flags;

		if (mp->mnt_flag & MNT_RDONLY)
			flags = FREAD;
		else
			flags = FREAD|FWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_OPEN(devvp, flags, FSCRED);
		VOP_UNLOCK(devvp);
		if (error)
			goto fail;
		error = lfs_mountfs(devvp, mp, l);		/* LFS */
		if (error) {
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			(void)VOP_CLOSE(devvp, flags, NOCRED);
			VOP_UNLOCK(devvp);
			goto fail;
		}

		ump = VFSTOUFS(mp);
		fs = ump->um_lfs;
	} else {
		/*
		 * Update the mount.
		 */

		/*
		 * The initial mount got a reference on this
		 * device, so drop the one obtained via
		 * namei(), above.
		 */
		vrele(devvp);

		ump = VFSTOUFS(mp);
		fs = ump->um_lfs;
		if (fs->lfs_ronly && (mp->mnt_iflag & IMNT_WANTRDWR)) {
			/*
			 * Changing from read-only to read/write.
			 * Note in the superblocks that we're writing.
			 */
			fs->lfs_ronly = 0;
			if (fs->lfs_pflags & LFS_PF_CLEAN) {
				fs->lfs_pflags &= ~LFS_PF_CLEAN;
				lfs_writesuper(fs, fs->lfs_sboffs[0]);
				lfs_writesuper(fs, fs->lfs_sboffs[1]);
			}
		}
		if (args->fspec == NULL)
			return EINVAL;
	}

	error = set_statvfs_info(path, UIO_USERSPACE, args->fspec,
	    UIO_USERSPACE, mp->mnt_op->vfs_name, mp, l);
	if (error == 0)
		(void)strncpy(fs->lfs_fsmnt, mp->mnt_stat.f_mntonname,
			      sizeof(fs->lfs_fsmnt));
	return error;

fail:
	vrele(devvp);
	return (error);
}


/*
 * Common code for mount and mountroot
 * LFS specific
 */
int
lfs_mountfs(struct vnode *devvp, struct mount *mp, struct lwp *l)
{
	struct dlfs *tdfs, *dfs, *adfs;
	struct lfs *fs;
	struct ufsmount *ump;
	struct vnode *vp;
	struct buf *bp, *abp;
	dev_t dev;
	int error, i, ronly, fsbsize;
	kauth_cred_t cred;
	CLEANERINFO *cip;
	SEGUSE *sup;
	daddr_t sb_addr;

	cred = l ? l->l_cred : NOCRED;

	/*
	 * Flush out any old buffers remaining from a previous use.
	 */
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	error = vinvalbuf(devvp, V_SAVE, cred, l, 0, 0);
	VOP_UNLOCK(devvp);
	if (error)
		return (error);

	ronly = (mp->mnt_flag & MNT_RDONLY) != 0;

	/* Don't free random space on error. */
	bp = NULL;
	abp = NULL;
	ump = NULL;

	sb_addr = LFS_LABELPAD / DEV_BSIZE;
	while (1) {
		/* Read in the superblock. */
		error = bread(devvp, sb_addr, LFS_SBPAD, cred, 0, &bp);
		if (error)
			goto out;
		dfs = (struct dlfs *)bp->b_data;

		/* Check the basics. */
		if (dfs->dlfs_magic != LFS_MAGIC || dfs->dlfs_bsize > MAXBSIZE ||
		    dfs->dlfs_version > LFS_VERSION ||
		    dfs->dlfs_bsize < sizeof(struct dlfs)) {
			DLOG((DLOG_MOUNT, "lfs_mountfs: primary superblock sanity failed\n"));
			error = EINVAL;		/* XXX needs translation */
			goto out;
		}
		if (dfs->dlfs_inodefmt > LFS_MAXINODEFMT) {
			DLOG((DLOG_MOUNT, "lfs_mountfs: unknown inode format %d\n",
			       dfs->dlfs_inodefmt));
			error = EINVAL;
			goto out;
		}

		if (dfs->dlfs_version == 1)
			fsbsize = DEV_BSIZE;
		else {
			fsbsize = 1 << dfs->dlfs_ffshift;
			/*
			 * Could be, if the frag size is large enough, that we
			 * don't have the "real" primary superblock.  If that's
			 * the case, get the real one, and try again.
			 */
			if (sb_addr != (dfs->dlfs_sboffs[0] << (dfs->dlfs_ffshift - DEV_BSHIFT))) {
				DLOG((DLOG_MOUNT, "lfs_mountfs: sb daddr"
				      " 0x%llx is not right, trying 0x%llx\n",
				      (long long)sb_addr,
				      (long long)(dfs->dlfs_sboffs[0] << (dfs->dlfs_ffshift - DEV_BSHIFT))));
				sb_addr = dfs->dlfs_sboffs[0] << (dfs->dlfs_ffshift - DEV_BSHIFT);
				brelse(bp, 0);
				continue;
			}
		}
		break;
	}

	/*
	 * Check the second superblock to see which is newer; then mount
	 * using the older of the two.	This is necessary to ensure that
	 * the filesystem is valid if it was not unmounted cleanly.
	 */

	if (dfs->dlfs_sboffs[1] &&
	    dfs->dlfs_sboffs[1] - LFS_LABELPAD / fsbsize > LFS_SBPAD / fsbsize)
	{
		error = bread(devvp, dfs->dlfs_sboffs[1] * (fsbsize / DEV_BSIZE),
			LFS_SBPAD, cred, 0, &abp);
		if (error)
			goto out;
		adfs = (struct dlfs *)abp->b_data;

		if (dfs->dlfs_version == 1) {
			/* 1s resolution comparison */
			if (adfs->dlfs_tstamp < dfs->dlfs_tstamp)
				tdfs = adfs;
			else
				tdfs = dfs;
		} else {
			/* monotonic infinite-resolution comparison */
			if (adfs->dlfs_serial < dfs->dlfs_serial)
				tdfs = adfs;
			else
				tdfs = dfs;
		}

		/* Check the basics. */
		if (tdfs->dlfs_magic != LFS_MAGIC ||
		    tdfs->dlfs_bsize > MAXBSIZE ||
		    tdfs->dlfs_version > LFS_VERSION ||
		    tdfs->dlfs_bsize < sizeof(struct dlfs)) {
			DLOG((DLOG_MOUNT, "lfs_mountfs: alt superblock"
			      " sanity failed\n"));
			error = EINVAL;		/* XXX needs translation */
			goto out;
		}
	} else {
		DLOG((DLOG_MOUNT, "lfs_mountfs: invalid alt superblock"
		      " daddr=0x%x\n", dfs->dlfs_sboffs[1]));
		error = EINVAL;
		goto out;
	}

	/* Allocate the mount structure, copy the superblock into it. */
	fs = malloc(sizeof(struct lfs), M_UFSMNT, M_WAITOK | M_ZERO);
	memcpy(&fs->lfs_dlfs, tdfs, sizeof(struct dlfs));

	/* Compatibility */
	if (fs->lfs_version < 2) {
		fs->lfs_sumsize = LFS_V1_SUMMARY_SIZE;
		fs->lfs_ibsize = fs->lfs_bsize;
		fs->lfs_start = fs->lfs_sboffs[0];
		fs->lfs_tstamp = fs->lfs_otstamp;
		fs->lfs_fsbtodb = 0;
	}
	if (fs->lfs_resvseg == 0)
		fs->lfs_resvseg = MIN(fs->lfs_minfreeseg - 1, \
			MAX(MIN_RESV_SEGS, fs->lfs_minfreeseg / 2 + 1));

	/*
	 * If we aren't going to be able to write meaningfully to this
	 * filesystem, and were not mounted readonly, bomb out now.
	 */
	if (fsbtob(fs, LFS_NRESERVE(fs)) > LFS_MAX_BYTES && !ronly) {
		DLOG((DLOG_MOUNT, "lfs_mount: to mount this filesystem read/write,"
		      " we need BUFPAGES >= %lld\n",
		      (long long)((bufmem_hiwater / bufmem_lowater) *
				  LFS_INVERSE_MAX_BYTES(
					  fsbtob(fs, LFS_NRESERVE(fs))) >> PAGE_SHIFT)));
		free(fs, M_UFSMNT);
		error = EFBIG; /* XXX needs translation */
		goto out;
	}

	/* Before rolling forward, lock so vget will sleep for other procs */
	if (l != NULL) {
		fs->lfs_flags = LFS_NOTYET;
		fs->lfs_rfpid = l->l_proc->p_pid;
	}

	ump = malloc(sizeof *ump, M_UFSMNT, M_WAITOK | M_ZERO);
	ump->um_lfs = fs;
	ump->um_ops = &lfs_ufsops;
	ump->um_fstype = UFS1;
	if (sizeof(struct lfs) < LFS_SBPAD) {			/* XXX why? */
		brelse(bp, BC_INVAL);
		brelse(abp, BC_INVAL);
	} else {
		brelse(bp, 0);
		brelse(abp, 0);
	}
	bp = NULL;
	abp = NULL;


	/* Set up the I/O information */
	fs->lfs_devbsize = DEV_BSIZE;
	fs->lfs_iocount = 0;
	fs->lfs_diropwait = 0;
	fs->lfs_activesb = 0;
	fs->lfs_uinodes = 0;
	fs->lfs_ravail = 0;
	fs->lfs_favail = 0;
	fs->lfs_sbactive = 0;

	/* Set up the ifile and lock aflags */
	fs->lfs_doifile = 0;
	fs->lfs_writer = 0;
	fs->lfs_dirops = 0;
	fs->lfs_nadirop = 0;
	fs->lfs_seglock = 0;
	fs->lfs_pdflush = 0;
	fs->lfs_sleepers = 0;
	fs->lfs_pages = 0;
	rw_init(&fs->lfs_fraglock);
	rw_init(&fs->lfs_iflock);
	cv_init(&fs->lfs_stopcv, "lfsstop");

	/* Set the file system readonly/modify bits. */
	fs->lfs_ronly = ronly;
	if (ronly == 0)
		fs->lfs_fmod = 1;

	/* Initialize the mount structure. */
	dev = devvp->v_rdev;
	mp->mnt_data = ump;
	mp->mnt_stat.f_fsidx.__fsid_val[0] = (long)dev;
	mp->mnt_stat.f_fsidx.__fsid_val[1] = makefstype(MOUNT_LFS);
	mp->mnt_stat.f_fsid = mp->mnt_stat.f_fsidx.__fsid_val[0];
	mp->mnt_stat.f_namemax = LFS_MAXNAMLEN;
	mp->mnt_stat.f_iosize = fs->lfs_bsize;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_fs_bshift = fs->lfs_bshift;
	ump->um_flags = 0;
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	ump->um_bptrtodb = fs->lfs_ffshift - DEV_BSHIFT;
	ump->um_seqinc = fs->lfs_frag;
	ump->um_nindir = fs->lfs_nindir;
	ump->um_lognindir = ffs(fs->lfs_nindir) - 1;
	for (i = 0; i < MAXQUOTAS; i++)
		ump->um_quotas[i] = NULLVP;
	ump->um_maxsymlinklen = fs->lfs_maxsymlinklen;
	ump->um_dirblksiz = DIRBLKSIZ;
	ump->um_maxfilesize = fs->lfs_maxfilesize;
	if (ump->um_maxsymlinklen > 0)
		mp->mnt_iflag |= IMNT_DTYPE;
	devvp->v_specmountpoint = mp;

	/* Set up reserved memory for pageout */
	lfs_setup_resblks(fs);
	/* Set up vdirop tailq */
	TAILQ_INIT(&fs->lfs_dchainhd);
	/* and paging tailq */
	TAILQ_INIT(&fs->lfs_pchainhd);
	/* and delayed segment accounting for truncation list */
	LIST_INIT(&fs->lfs_segdhd);

	/*
	 * We use the ifile vnode for almost every operation.  Instead of
	 * retrieving it from the hash table each time we retrieve it here,
	 * artificially increment the reference count and keep a pointer
	 * to it in the incore copy of the superblock.
	 */
	if ((error = VFS_VGET(mp, LFS_IFILE_INUM, &vp)) != 0) {
		DLOG((DLOG_MOUNT, "lfs_mountfs: ifile vget failed, error=%d\n", error));
		goto out;
	}
	fs->lfs_ivnode = vp;
	vref(vp);

	/* Set up inode bitmap and order free list */
	lfs_order_freelist(fs);

	/* Set up segment usage flags for the autocleaner. */
	fs->lfs_nactive = 0;
	fs->lfs_suflags = (u_int32_t **)malloc(2 * sizeof(u_int32_t *),
						M_SEGMENT, M_WAITOK);
	fs->lfs_suflags[0] = (u_int32_t *)malloc(fs->lfs_nseg * sizeof(u_int32_t),
						 M_SEGMENT, M_WAITOK);
	fs->lfs_suflags[1] = (u_int32_t *)malloc(fs->lfs_nseg * sizeof(u_int32_t),
						 M_SEGMENT, M_WAITOK);
	memset(fs->lfs_suflags[1], 0, fs->lfs_nseg * sizeof(u_int32_t));
	for (i = 0; i < fs->lfs_nseg; i++) {
		int changed;

		LFS_SEGENTRY(sup, fs, i, bp);
		changed = 0;
		if (!ronly) {
			if (sup->su_nbytes == 0 &&
			    !(sup->su_flags & SEGUSE_EMPTY)) {
				sup->su_flags |= SEGUSE_EMPTY;
				++changed;
			} else if (!(sup->su_nbytes == 0) &&
				   (sup->su_flags & SEGUSE_EMPTY)) {
				sup->su_flags &= ~SEGUSE_EMPTY;
				++changed;
			}
			if (sup->su_flags & (SEGUSE_ACTIVE|SEGUSE_INVAL)) {
				sup->su_flags &= ~(SEGUSE_ACTIVE|SEGUSE_INVAL);
				++changed;
			}
		}
		fs->lfs_suflags[0][i] = sup->su_flags;
		if (changed)
			LFS_WRITESEGENTRY(sup, fs, i, bp);
		else
			brelse(bp, 0);
	}

#ifdef LFS_KERNEL_RFW
	lfs_roll_forward(fs, mp, l);
#endif

	/* If writing, sb is not clean; record in case of immediate crash */
	if (!fs->lfs_ronly) {
		fs->lfs_pflags &= ~LFS_PF_CLEAN;
		lfs_writesuper(fs, fs->lfs_sboffs[0]);
		lfs_writesuper(fs, fs->lfs_sboffs[1]);
	}

	/* Allow vget now that roll-forward is complete */
	fs->lfs_flags &= ~(LFS_NOTYET);
	wakeup(&fs->lfs_flags);

	/*
	 * Initialize the ifile cleaner info with information from
	 * the superblock.
	 */
	LFS_CLEANERINFO(cip, fs, bp);
	cip->clean = fs->lfs_nclean;
	cip->dirty = fs->lfs_nseg - fs->lfs_nclean;
	cip->avail = fs->lfs_avail;
	cip->bfree = fs->lfs_bfree;
	(void) LFS_BWRITE_LOG(bp); /* Ifile */

	/*
	 * Mark the current segment as ACTIVE, since we're going to
	 * be writing to it.
	 */
	LFS_SEGENTRY(sup, fs, dtosn(fs, fs->lfs_offset), bp);
	sup->su_flags |= SEGUSE_DIRTY | SEGUSE_ACTIVE;
	fs->lfs_nactive++;
	LFS_WRITESEGENTRY(sup, fs, dtosn(fs, fs->lfs_offset), bp);  /* Ifile */

	/* Now that roll-forward is done, unlock the Ifile */
	vput(vp);

	/* Start the pagedaemon-anticipating daemon */
	if (lfs_writer_daemon == 0 && kthread_create(PRI_BIO, 0, NULL,
	    lfs_writerd, NULL, NULL, "lfs_writer") != 0)
		panic("fork lfs_writer");
	/*
	 * XXX: Get extra reference to LFS vfsops.  This prevents unload,
	 * but also prevents kernel panic due to text being unloaded
	 * from below lfs_writerd.  When lfs_writerd can exit, remove
	 * this!!!
	 */
	vfs_getopsbyname(MOUNT_LFS);

	printf("WARNING: the log-structured file system is experimental\n"
	    "WARNING: it may cause system crashes and/or corrupt data\n");

	return (0);

out:
	if (bp)
		brelse(bp, 0);
	if (abp)
		brelse(abp, 0);
	if (ump) {
		free(ump->um_lfs, M_UFSMNT);
		free(ump, M_UFSMNT);
		mp->mnt_data = NULL;
	}

	return (error);
}

/*
 * unmount system call
 */
int
lfs_unmount(struct mount *mp, int mntflags)
{
	struct lwp *l = curlwp;
	struct ufsmount *ump;
	struct lfs *fs;
	int error, flags, ronly;
	vnode_t *vp;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	ump = VFSTOUFS(mp);
	fs = ump->um_lfs;

	/* Two checkpoints */
	lfs_segwrite(mp, SEGM_CKP | SEGM_SYNC);
	lfs_segwrite(mp, SEGM_CKP | SEGM_SYNC);

	/* wake up the cleaner so it can die */
	lfs_wakeup_cleaner(fs);
	mutex_enter(&lfs_lock);
	while (fs->lfs_sleepers)
		mtsleep(&fs->lfs_sleepers, PRIBIO + 1, "lfs_sleepers", 0,
			&lfs_lock);
	mutex_exit(&lfs_lock);

#ifdef QUOTA
        if ((error = quota1_umount(mp, flags)) != 0)
		return (error);
#endif
	if ((error = vflush(mp, fs->lfs_ivnode, flags)) != 0)
		return (error);
	if ((error = VFS_SYNC(mp, 1, l->l_cred)) != 0)
		return (error);
	vp = fs->lfs_ivnode;
	mutex_enter(vp->v_interlock);
	if (LIST_FIRST(&vp->v_dirtyblkhd))
		panic("lfs_unmount: still dirty blocks on ifile vnode");
	mutex_exit(vp->v_interlock);

	/* Explicitly write the superblock, to update serial and pflags */
	fs->lfs_pflags |= LFS_PF_CLEAN;
	lfs_writesuper(fs, fs->lfs_sboffs[0]);
	lfs_writesuper(fs, fs->lfs_sboffs[1]);
	mutex_enter(&lfs_lock);
	while (fs->lfs_iocount)
		mtsleep(&fs->lfs_iocount, PRIBIO + 1, "lfs_umount", 0,
			&lfs_lock);
	mutex_exit(&lfs_lock);

	/* Finish with the Ifile, now that we're done with it */
	vgone(fs->lfs_ivnode);

	ronly = !fs->lfs_ronly;
	if (ump->um_devvp->v_type != VBAD)
		ump->um_devvp->v_specmountpoint = NULL;
	vn_lock(ump->um_devvp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_CLOSE(ump->um_devvp,
	    ronly ? FREAD : FREAD|FWRITE, NOCRED);
	vput(ump->um_devvp);

	/* Complain about page leakage */
	if (fs->lfs_pages > 0)
		printf("lfs_unmount: still claim %d pages (%d in subsystem)\n",
			fs->lfs_pages, lfs_subsys_pages);

	/* Free per-mount data structures */
	free(fs->lfs_ino_bitmap, M_SEGMENT);
	free(fs->lfs_suflags[0], M_SEGMENT);
	free(fs->lfs_suflags[1], M_SEGMENT);
	free(fs->lfs_suflags, M_SEGMENT);
	lfs_free_resblks(fs);
	cv_destroy(&fs->lfs_stopcv);
	rw_destroy(&fs->lfs_fraglock);
	rw_destroy(&fs->lfs_iflock);
	free(fs, M_UFSMNT);
	free(ump, M_UFSMNT);

	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (error);
}

/*
 * Get file system statistics.
 *
 * NB: We don't lock to access the superblock here, because it's not
 * really that important if we get it wrong.
 */
int
lfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct lfs *fs;
	struct ufsmount *ump;

	ump = VFSTOUFS(mp);
	fs = ump->um_lfs;
	if (fs->lfs_magic != LFS_MAGIC)
		panic("lfs_statvfs: magic");

	sbp->f_bsize = fs->lfs_bsize;
	sbp->f_frsize = fs->lfs_fsize;
	sbp->f_iosize = fs->lfs_bsize;
	sbp->f_blocks = LFS_EST_NONMETA(fs) - VTOI(fs->lfs_ivnode)->i_lfs_effnblks;

	sbp->f_bfree = LFS_EST_BFREE(fs);
	KASSERT(sbp->f_bfree <= fs->lfs_dsize);
#if 0
	if (sbp->f_bfree < 0)
		sbp->f_bfree = 0;
#endif

	sbp->f_bresvd = LFS_EST_RSVD(fs);
	if (sbp->f_bfree > sbp->f_bresvd)
		sbp->f_bavail = sbp->f_bfree - sbp->f_bresvd;
	else
		sbp->f_bavail = 0;

	sbp->f_files = fs->lfs_bfree / btofsb(fs, fs->lfs_ibsize) * INOPB(fs);
	sbp->f_ffree = sbp->f_files - fs->lfs_nfiles;
	sbp->f_favail = sbp->f_ffree;
	sbp->f_fresvd = 0;
	copy_statvfs_info(sbp, mp);
	return (0);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
int
lfs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	int error;
	struct lfs *fs;

	fs = VFSTOUFS(mp)->um_lfs;
	if (fs->lfs_ronly)
		return 0;

	/* Snapshots should not hose the syncer */
	/*
	 * XXX Sync can block here anyway, since we don't have a very
	 * XXX good idea of how much data is pending.  If it's more
	 * XXX than a segment and lfs_nextseg is close to the end of
	 * XXX the log, we'll likely block.
	 */
	mutex_enter(&lfs_lock);
	if (fs->lfs_nowrap && fs->lfs_nextseg < fs->lfs_curseg) {
		mutex_exit(&lfs_lock);
		return 0;
	}
	mutex_exit(&lfs_lock);

	lfs_writer_enter(fs, "lfs_dirops");

	/* All syncs must be checkpoints until roll-forward is implemented. */
	DLOG((DLOG_FLUSH, "lfs_sync at 0x%x\n", fs->lfs_offset));
	error = lfs_segwrite(mp, SEGM_CKP | (waitfor ? SEGM_SYNC : 0));
	lfs_writer_leave(fs);
#ifdef QUOTA
	qsync(mp);
#endif
	return (error);
}

/*
 * Look up an LFS dinode number to find its incore vnode.  If not already
 * in core, read it in from the specified device.  Return the inode locked.
 * Detection and handling of mount points must be done by the calling routine.
 */
int
lfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct lfs *fs;
	struct ufs1_dinode *dip;
	struct inode *ip;
	struct buf *bp;
	struct ifile *ifp;
	struct vnode *vp;
	struct ufsmount *ump;
	daddr_t daddr;
	dev_t dev;
	int error, retries;
	struct timespec ts;

	memset(&ts, 0, sizeof ts);	/* XXX gcc */

	ump = VFSTOUFS(mp);
	dev = ump->um_dev;
	fs = ump->um_lfs;

	/*
	 * If the filesystem is not completely mounted yet, suspend
	 * any access requests (wait for roll-forward to complete).
	 */
	mutex_enter(&lfs_lock);
	while ((fs->lfs_flags & LFS_NOTYET) && curproc->p_pid != fs->lfs_rfpid)
		mtsleep(&fs->lfs_flags, PRIBIO+1, "lfs_notyet", 0,
			&lfs_lock);
	mutex_exit(&lfs_lock);

retry:
	if ((*vpp = ufs_ihashget(dev, ino, LK_EXCLUSIVE)) != NULL)
		return (0);

	error = getnewvnode(VT_LFS, mp, lfs_vnodeop_p, NULL, &vp);
	if (error) {
		*vpp = NULL;
		 return (error);
	}

	mutex_enter(&ufs_hashlock);
	if (ufs_ihashget(dev, ino, 0) != NULL) {
		mutex_exit(&ufs_hashlock);
		ungetnewvnode(vp);
		goto retry;
	}

	/* Translate the inode number to a disk address. */
	if (ino == LFS_IFILE_INUM)
		daddr = fs->lfs_idaddr;
	else {
		/* XXX bounds-check this too */
		LFS_IENTRY(ifp, fs, ino, bp);
		daddr = ifp->if_daddr;
		if (fs->lfs_version > 1) {
			ts.tv_sec = ifp->if_atime_sec;
			ts.tv_nsec = ifp->if_atime_nsec;
		}

		brelse(bp, 0);
		if (daddr == LFS_UNUSED_DADDR) {
			*vpp = NULLVP;
			mutex_exit(&ufs_hashlock);
			ungetnewvnode(vp);
			return (ENOENT);
		}
	}

	/* Allocate/init new vnode/inode. */
	lfs_vcreate(mp, ino, vp);

	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	ip = VTOI(vp);
	ufs_ihashins(ip);
	mutex_exit(&ufs_hashlock);

	/*
	 * XXX
	 * This may not need to be here, logically it should go down with
	 * the i_devvp initialization.
	 * Ask Kirk.
	 */
	ip->i_lfs = ump->um_lfs;

	/* Read in the disk contents for the inode, copy into the inode. */
	retries = 0;
    again:
	error = bread(ump->um_devvp, fsbtodb(fs, daddr),
		(fs->lfs_version == 1 ? fs->lfs_bsize : fs->lfs_ibsize),
		NOCRED, 0, &bp);
	if (error) {
		/*
		 * The inode does not contain anything useful, so it would
		 * be misleading to leave it on its hash chain. With mode
		 * still zero, it will be unlinked and returned to the free
		 * list by vput().
		 */
		vput(vp);
		brelse(bp, 0);
		*vpp = NULL;
		return (error);
	}

	dip = lfs_ifind(fs, ino, bp);
	if (dip == NULL) {
		/* Assume write has not completed yet; try again */
		brelse(bp, BC_INVAL);
		++retries;
		if (retries > LFS_IFIND_RETRIES) {
#ifdef DEBUG
			/* If the seglock is held look at the bpp to see
			   what is there anyway */
			mutex_enter(&lfs_lock);
			if (fs->lfs_seglock > 0) {
				struct buf **bpp;
				struct ufs1_dinode *dp;
				int i;

				for (bpp = fs->lfs_sp->bpp;
				     bpp != fs->lfs_sp->cbpp; ++bpp) {
					if ((*bpp)->b_vp == fs->lfs_ivnode &&
					    bpp != fs->lfs_sp->bpp) {
						/* Inode block */
						printf("lfs_vget: block 0x%" PRIx64 ": ",
						       (*bpp)->b_blkno);
						dp = (struct ufs1_dinode *)(*bpp)->b_data;
						for (i = 0; i < INOPB(fs); i++)
							if (dp[i].di_u.inumber)
								printf("%d ", dp[i].di_u.inumber);
						printf("\n");
					}
				}
			}
			mutex_exit(&lfs_lock);
#endif /* DEBUG */
			panic("lfs_vget: dinode not found");
		}
		mutex_enter(&lfs_lock);
		if (fs->lfs_iocount) {
			DLOG((DLOG_VNODE, "lfs_vget: dinode %d not found, retrying...\n", ino));
			(void)mtsleep(&fs->lfs_iocount, PRIBIO + 1,
				      "lfs ifind", 1, &lfs_lock);
		} else
			retries = LFS_IFIND_RETRIES;
		mutex_exit(&lfs_lock);
		goto again;
	}
	*ip->i_din.ffs1_din = *dip;
	brelse(bp, 0);

	if (fs->lfs_version > 1) {
		ip->i_ffs1_atime = ts.tv_sec;
		ip->i_ffs1_atimensec = ts.tv_nsec;
	}

	lfs_vinit(mp, &vp);

	*vpp = vp;

	KASSERT(VOP_ISLOCKED(vp));

	return (0);
}

/*
 * File handle to vnode
 */
int
lfs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct lfid lfh;
	struct buf *bp;
	IFILE *ifp;
	int32_t daddr;
	struct lfs *fs;
	vnode_t *vp;

	if (fhp->fid_len != sizeof(struct lfid))
		return EINVAL;

	memcpy(&lfh, fhp, sizeof(lfh));
	if (lfh.lfid_ino < LFS_IFILE_INUM)
		return ESTALE;

	fs = VFSTOUFS(mp)->um_lfs;
	if (lfh.lfid_ident != fs->lfs_ident)
		return ESTALE;

	if (lfh.lfid_ino >
	    ((VTOI(fs->lfs_ivnode)->i_ffs1_size >> fs->lfs_bshift) -
	     fs->lfs_cleansz - fs->lfs_segtabsz) * fs->lfs_ifpb)
		return ESTALE;

	mutex_enter(&ufs_ihash_lock);
	vp = ufs_ihashlookup(VFSTOUFS(mp)->um_dev, lfh.lfid_ino);
	mutex_exit(&ufs_ihash_lock);
	if (vp == NULL) {
		LFS_IENTRY(ifp, fs, lfh.lfid_ino, bp);
		daddr = ifp->if_daddr;
		brelse(bp, 0);
		if (daddr == LFS_UNUSED_DADDR)
			return ESTALE;
	}

	return (ufs_fhtovp(mp, &lfh.lfid_ufid, vpp));
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
lfs_vptofh(struct vnode *vp, struct fid *fhp, size_t *fh_size)
{
	struct inode *ip;
	struct lfid lfh;

	if (*fh_size < sizeof(struct lfid)) {
		*fh_size = sizeof(struct lfid);
		return E2BIG;
	}
	*fh_size = sizeof(struct lfid);
	ip = VTOI(vp);
	memset(&lfh, 0, sizeof(lfh));
	lfh.lfid_len = sizeof(struct lfid);
	lfh.lfid_ino = ip->i_number;
	lfh.lfid_gen = ip->i_gen;
	lfh.lfid_ident = ip->i_lfs->lfs_ident;
	memcpy(fhp, &lfh, sizeof(lfh));
	return (0);
}

/*
 * ufs_bmaparray callback function for writing.
 *
 * Since blocks will be written to the new segment anyway,
 * we don't care about current daddr of them.
 */
static bool
lfs_issequential_hole(const struct ufsmount *ump,
    daddr_t daddr0, daddr_t daddr1)
{
	daddr0 = (daddr_t)((int32_t)daddr0); /* XXX ondisk32 */
	daddr1 = (daddr_t)((int32_t)daddr1); /* XXX ondisk32 */

	KASSERT(daddr0 == UNWRITTEN ||
	    (0 <= daddr0 && daddr0 <= LFS_MAX_DADDR));
	KASSERT(daddr1 == UNWRITTEN ||
	    (0 <= daddr1 && daddr1 <= LFS_MAX_DADDR));

	/* NOTE: all we want to know here is 'hole or not'. */
	/* NOTE: UNASSIGNED is converted to 0 by ufs_bmaparray. */

	/*
	 * treat UNWRITTENs and all resident blocks as 'contiguous'
	 */
	if (daddr0 != 0 && daddr1 != 0)
		return true;

	/*
	 * both are in hole?
	 */
	if (daddr0 == 0 && daddr1 == 0)
		return true; /* all holes are 'contiguous' for us. */

	return false;
}

/*
 * lfs_gop_write functions exactly like genfs_gop_write, except that
 * (1) it requires the seglock to be held by its caller, and sp->fip
 *     to be properly initialized (it will return without re-initializing
 *     sp->fip, and without calling lfs_writeseg).
 * (2) it uses the remaining space in the segment, rather than VOP_BMAP,
 *     to determine how large a block it can write at once (though it does
 *     still use VOP_BMAP to find holes in the file);
 * (3) it calls lfs_gatherblock instead of VOP_STRATEGY on its blocks
 *     (leaving lfs_writeseg to deal with the cluster blocks, so we might
 *     now have clusters of clusters, ick.)
 */
static int
lfs_gop_write(struct vnode *vp, struct vm_page **pgs, int npages,
    int flags)
{
	int i, error, run, haveeof = 0;
	int fs_bshift;
	vaddr_t kva;
	off_t eof, offset, startoffset = 0;
	size_t bytes, iobytes, skipbytes;
	bool async = (flags & PGO_SYNCIO) == 0;
	daddr_t lbn, blkno;
	struct vm_page *pg;
	struct buf *mbp, *bp;
	struct vnode *devvp = VTOI(vp)->i_devvp;
	struct inode *ip = VTOI(vp);
	struct lfs *fs = ip->i_lfs;
	struct segment *sp = fs->lfs_sp;
	UVMHIST_FUNC("lfs_gop_write"); UVMHIST_CALLED(ubchist);

	ASSERT_SEGLOCK(fs);

	/* The Ifile lives in the buffer cache */
	KASSERT(vp != fs->lfs_ivnode);

	/*
	 * We don't want to fill the disk before the cleaner has a chance
	 * to make room for us.  If we're in danger of doing that, fail
	 * with EAGAIN.  The caller will have to notice this, unlock
	 * so the cleaner can run, relock and try again.
	 *
	 * We must write everything, however, if our vnode is being
	 * reclaimed.
	 */
	if (LFS_STARVED_FOR_SEGS(fs) && vp != fs->lfs_flushvp)
		goto tryagain;

	/*
	 * Sometimes things slip past the filters in lfs_putpages,
	 * and the pagedaemon tries to write pages---problem is
	 * that the pagedaemon never acquires the segment lock.
	 *
	 * Alternatively, pages that were clean when we called
	 * genfs_putpages may have become dirty in the meantime.  In this
	 * case the segment header is not properly set up for blocks
	 * to be added to it.
	 *
	 * Unbusy and unclean the pages, and put them on the ACTIVE
	 * queue under the hypothesis that they couldn't have got here
	 * unless they were modified *quite* recently.
	 *
	 * XXXUBC that last statement is an oversimplification of course.
	 */
	if (!LFS_SEGLOCK_HELD(fs) ||
	    (ip->i_lfs_iflags & LFSI_NO_GOP_WRITE) ||
	    (pgs[0]->offset & fs->lfs_bmask) != 0) {
		goto tryagain;
	}

	UVMHIST_LOG(ubchist, "vp %p pgs %p npages %d flags 0x%x",
	    vp, pgs, npages, flags);

	GOP_SIZE(vp, vp->v_size, &eof, 0);
	haveeof = 1;

	if (vp->v_type == VREG)
		fs_bshift = vp->v_mount->mnt_fs_bshift;
	else
		fs_bshift = DEV_BSHIFT;
	error = 0;
	pg = pgs[0];
	startoffset = pg->offset;
	KASSERT(eof >= 0);

	if (startoffset >= eof) {
		goto tryagain;
	} else
		bytes = MIN(npages << PAGE_SHIFT, eof - startoffset);
	skipbytes = 0;

	KASSERT(bytes != 0);

	/* Swap PG_DELWRI for PG_PAGEOUT */
	for (i = 0; i < npages; i++) {
		if (pgs[i]->flags & PG_DELWRI) {
			KASSERT(!(pgs[i]->flags & PG_PAGEOUT));
			pgs[i]->flags &= ~PG_DELWRI;
			pgs[i]->flags |= PG_PAGEOUT;
			uvm_pageout_start(1);
			mutex_enter(&uvm_pageqlock);
			uvm_pageunwire(pgs[i]);
			mutex_exit(&uvm_pageqlock);
		}
	}

	/*
	 * Check to make sure we're starting on a block boundary.
	 * We'll check later to make sure we always write entire
	 * blocks (or fragments).
	 */
	if (startoffset & fs->lfs_bmask)
		printf("%" PRId64 " & %" PRId64 " = %" PRId64 "\n",
		       startoffset, fs->lfs_bmask,
		       startoffset & fs->lfs_bmask);
	KASSERT((startoffset & fs->lfs_bmask) == 0);
	if (bytes & fs->lfs_ffmask) {
		printf("lfs_gop_write: asked to write %ld bytes\n", (long)bytes);
		panic("lfs_gop_write: non-integer blocks");
	}

	/*
	 * We could deadlock here on pager_map with UVMPAGER_MAPIN_WAITOK.
	 * If we would, write what we have and try again.  If we don't
	 * have anything to write, we'll have to sleep.
	 */
	if ((kva = uvm_pagermapin(pgs, npages, UVMPAGER_MAPIN_WRITE |
				      (((SEGSUM *)(sp->segsum))->ss_nfinfo < 1 ?
				       UVMPAGER_MAPIN_WAITOK : 0))) == 0x0) {
		DLOG((DLOG_PAGE, "lfs_gop_write: forcing write\n"));
#if 0
		      " with nfinfo=%d at offset 0x%x\n",
		      (int)((SEGSUM *)(sp->segsum))->ss_nfinfo,
		      (unsigned)fs->lfs_offset));
#endif
		lfs_updatemeta(sp);
		lfs_release_finfo(fs);
		(void) lfs_writeseg(fs, sp);

		lfs_acquire_finfo(fs, ip->i_number, ip->i_gen);

		/*
		 * Having given up all of the pager_map we were holding,
		 * we can now wait for aiodoned to reclaim it for us
		 * without fear of deadlock.
		 */
		kva = uvm_pagermapin(pgs, npages, UVMPAGER_MAPIN_WRITE |
				     UVMPAGER_MAPIN_WAITOK);
	}

	mbp = getiobuf(NULL, true);
	UVMHIST_LOG(ubchist, "vp %p mbp %p num now %d bytes 0x%x",
	    vp, mbp, vp->v_numoutput, bytes);
	mbp->b_bufsize = npages << PAGE_SHIFT;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_cflags = BC_BUSY|BC_AGE;
	mbp->b_iodone = uvm_aio_biodone;

	bp = NULL;
	for (offset = startoffset;
	    bytes > 0;
	    offset += iobytes, bytes -= iobytes) {
		lbn = offset >> fs_bshift;
		error = ufs_bmaparray(vp, lbn, &blkno, NULL, NULL, &run,
		    lfs_issequential_hole);
		if (error) {
			UVMHIST_LOG(ubchist, "ufs_bmaparray() -> %d",
			    error,0,0,0);
			skipbytes += bytes;
			bytes = 0;
			break;
		}

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);
		if (blkno == (daddr_t)-1) {
			skipbytes += iobytes;
			continue;
		}

		/*
		 * Discover how much we can really pack into this buffer.
		 */
		/* If no room in the current segment, finish it up */
		if (sp->sum_bytes_left < sizeof(int32_t) ||
		    sp->seg_bytes_left < (1 << fs->lfs_bshift)) {
			int vers;

			lfs_updatemeta(sp);
			vers = sp->fip->fi_version;
			lfs_release_finfo(fs);
			(void) lfs_writeseg(fs, sp);

			lfs_acquire_finfo(fs, ip->i_number, vers);
		}
		/* Check both for space in segment and space in segsum */
		iobytes = MIN(iobytes, (sp->seg_bytes_left >> fs_bshift)
					<< fs_bshift);
		iobytes = MIN(iobytes, (sp->sum_bytes_left / sizeof(int32_t))
				       << fs_bshift);
		KASSERT(iobytes > 0);

		/* if it's really one i/o, don't make a second buf */
		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
			/* 
			 * All the LFS output is done by the segwriter.  It
			 * will increment numoutput by one for all the bufs it
			 * recieves.  However this buffer needs one extra to
			 * account for aiodone.
			 */
			mutex_enter(vp->v_interlock);
			vp->v_numoutput++;
			mutex_exit(vp->v_interlock);
		} else {
			bp = getiobuf(NULL, true);
			UVMHIST_LOG(ubchist, "vp %p bp %p num now %d",
			    vp, bp, vp->v_numoutput, 0);
			nestiobuf_setup(mbp, bp, offset - pg->offset, iobytes);
			/*
			 * LFS doesn't like async I/O here, dies with
			 * and assert in lfs_bwrite().  Is that assert
			 * valid?  I retained non-async behaviour when
			 * converted this to use nestiobuf --pooka
			 */
			bp->b_flags &= ~B_ASYNC;
		}

		/* XXX This is silly ... is this necessary? */
		mutex_enter(&bufcache_lock);
		mutex_enter(vp->v_interlock);
		bgetvp(vp, bp);
		mutex_exit(vp->v_interlock);
		mutex_exit(&bufcache_lock);

		bp->b_lblkno = lblkno(fs, offset);
		bp->b_private = mbp;
		if (devvp->v_type == VBLK) {
			bp->b_dev = devvp->v_rdev;
		}
		VOP_BWRITE(bp->b_vp, bp);
		while (lfs_gatherblock(sp, bp, NULL))
			continue;
	}

	nestiobuf_done(mbp, skipbytes, error);
	if (skipbytes) {
		UVMHIST_LOG(ubchist, "skipbytes %d", skipbytes, 0,0,0);
	}
	UVMHIST_LOG(ubchist, "returning 0", 0,0,0,0);

	if (!async) {
		/* Start a segment write. */
		UVMHIST_LOG(ubchist, "flushing", 0,0,0,0);
		mutex_enter(&lfs_lock);
		lfs_flush(fs, 0, 1);
		mutex_exit(&lfs_lock);
	}
	return (0);

    tryagain:
	/*
	 * We can't write the pages, for whatever reason.
	 * Clean up after ourselves, and make the caller try again.
	 */
	mutex_enter(vp->v_interlock);

	/* Tell why we're here, if we know */
	if (ip->i_lfs_iflags & LFSI_NO_GOP_WRITE) {
		DLOG((DLOG_PAGE, "lfs_gop_write: clean pages dirtied\n"));
	} else if ((pgs[0]->offset & fs->lfs_bmask) != 0) {
		DLOG((DLOG_PAGE, "lfs_gop_write: not on block boundary\n"));
	} else if (haveeof && startoffset >= eof) {
		DLOG((DLOG_PAGE, "lfs_gop_write: ino %d start 0x%" PRIx64
		      " eof 0x%" PRIx64 " npages=%d\n", VTOI(vp)->i_number,
		      pgs[0]->offset, eof, npages));
	} else if (LFS_STARVED_FOR_SEGS(fs)) {
		DLOG((DLOG_PAGE, "lfs_gop_write: avail too low\n"));
	} else {
		DLOG((DLOG_PAGE, "lfs_gop_write: seglock not held\n"));
	}

	mutex_enter(&uvm_pageqlock);
	for (i = 0; i < npages; i++) {
		pg = pgs[i];

		if (pg->flags & PG_PAGEOUT)
			uvm_pageout_done(1);
		if (pg->flags & PG_DELWRI) {
			uvm_pageunwire(pg);
		}
		uvm_pageactivate(pg);
		pg->flags &= ~(PG_CLEAN|PG_DELWRI|PG_PAGEOUT|PG_RELEASED);
		DLOG((DLOG_PAGE, "pg[%d] = %p (vp %p off %" PRIx64 ")\n", i, pg,
			vp, pg->offset));
		DLOG((DLOG_PAGE, "pg[%d]->flags = %x\n", i, pg->flags));
		DLOG((DLOG_PAGE, "pg[%d]->pqflags = %x\n", i, pg->pqflags));
		DLOG((DLOG_PAGE, "pg[%d]->uanon = %p\n", i, pg->uanon));
		DLOG((DLOG_PAGE, "pg[%d]->uobject = %p\n", i, pg->uobject));
		DLOG((DLOG_PAGE, "pg[%d]->wire_count = %d\n", i,
		      pg->wire_count));
		DLOG((DLOG_PAGE, "pg[%d]->loan_count = %d\n", i,
		      pg->loan_count));
	}
	/* uvm_pageunbusy takes care of PG_BUSY, PG_WANTED */
	uvm_page_unbusy(pgs, npages);
	mutex_exit(&uvm_pageqlock);
	mutex_exit(vp->v_interlock);
	return EAGAIN;
}

/*
 * finish vnode/inode initialization.
 * used by lfs_vget and lfs_fastvget.
 */
void
lfs_vinit(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp = *vpp;
	struct inode *ip = VTOI(vp);
	struct ufsmount *ump = VFSTOUFS(mp);
	struct lfs *fs = ump->um_lfs;
	int i;

	ip->i_mode = ip->i_ffs1_mode;
	ip->i_nlink = ip->i_ffs1_nlink;
	ip->i_lfs_osize = ip->i_size = ip->i_ffs1_size;
	ip->i_flags = ip->i_ffs1_flags;
	ip->i_gen = ip->i_ffs1_gen;
	ip->i_uid = ip->i_ffs1_uid;
	ip->i_gid = ip->i_ffs1_gid;

	ip->i_lfs_effnblks = ip->i_ffs1_blocks;
	ip->i_lfs_odnlink = ip->i_ffs1_nlink;

	/*
	 * Initialize the vnode from the inode, check for aliases.  In all
	 * cases re-init ip, the underlying vnode/inode may have changed.
	 */
	ufs_vinit(mp, lfs_specop_p, lfs_fifoop_p, &vp);
	ip = VTOI(vp);

	memset(ip->i_lfs_fragsize, 0, NDADDR * sizeof(*ip->i_lfs_fragsize));
	if (vp->v_type != VLNK || ip->i_size >= ip->i_ump->um_maxsymlinklen) {
#ifdef DEBUG
		for (i = (ip->i_size + fs->lfs_bsize - 1) >> fs->lfs_bshift;
		    i < NDADDR; i++) {
			if ((vp->v_type == VBLK || vp->v_type == VCHR) &&
			    i == 0)
				continue;
			if (ip->i_ffs1_db[i] != 0) {
inconsistent:
				lfs_dump_dinode(ip->i_din.ffs1_din);
				panic("inconsistent inode");
			}
		}
		for ( ; i < NDADDR + NIADDR; i++) {
			if (ip->i_ffs1_ib[i - NDADDR] != 0) {
				goto inconsistent;
			}
		}
#endif /* DEBUG */
		for (i = 0; i < NDADDR; i++)
			if (ip->i_ffs1_db[i] != 0)
				ip->i_lfs_fragsize[i] = blksize(fs, ip, i);
	}

#ifdef DIAGNOSTIC
	if (vp->v_type == VNON) {
# ifdef DEBUG
		lfs_dump_dinode(ip->i_din.ffs1_din);
# endif
		panic("lfs_vinit: ino %llu is type VNON! (ifmt=%o)\n",
		      (unsigned long long)ip->i_number,
		      (ip->i_mode & IFMT) >> 12);
	}
#endif /* DIAGNOSTIC */

	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */

	ip->i_devvp = ump->um_devvp;
	vref(ip->i_devvp);
	genfs_node_init(vp, &lfs_genfsops);
	uvm_vnp_setsize(vp, ip->i_size);

	/* Initialize hiblk from file size */
	ip->i_lfs_hiblk = lblkno(ip->i_lfs, ip->i_size + ip->i_lfs->lfs_bsize - 1) - 1;

	*vpp = vp;
}

/*
 * Resize the filesystem to contain the specified number of segments.
 */
int
lfs_resize_fs(struct lfs *fs, int newnsegs)
{
	SEGUSE *sup;
	struct buf *bp, *obp;
	daddr_t olast, nlast, ilast, noff, start, end;
	struct vnode *ivp;
	struct inode *ip;
	int error, badnews, inc, oldnsegs;
	int sbbytes, csbbytes, gain, cgain;
	int i;

	/* Only support v2 and up */
	if (fs->lfs_version < 2)
		return EOPNOTSUPP;

	/* If we're doing nothing, do it fast */
	oldnsegs = fs->lfs_nseg;
	if (newnsegs == oldnsegs)
		return 0;

	/* We always have to have two superblocks */
	if (newnsegs <= dtosn(fs, fs->lfs_sboffs[1]))
		return EFBIG;

	ivp = fs->lfs_ivnode;
	ip = VTOI(ivp);
	error = 0;

	/* Take the segment lock so no one else calls lfs_newseg() */
	lfs_seglock(fs, SEGM_PROT);

	/*
	 * Make sure the segments we're going to be losing, if any,
	 * are in fact empty.  We hold the seglock, so their status
	 * cannot change underneath us.  Count the superblocks we lose,
	 * while we're at it.
	 */
	sbbytes = csbbytes = 0;
	cgain = 0;
	for (i = newnsegs; i < oldnsegs; i++) {
		LFS_SEGENTRY(sup, fs, i, bp);
		badnews = sup->su_nbytes || !(sup->su_flags & SEGUSE_INVAL);
		if (sup->su_flags & SEGUSE_SUPERBLOCK)
			sbbytes += LFS_SBPAD;
		if (!(sup->su_flags & SEGUSE_DIRTY)) {
			++cgain;
			if (sup->su_flags & SEGUSE_SUPERBLOCK)
				csbbytes += LFS_SBPAD;
		}
		brelse(bp, 0);
		if (badnews) {
			error = EBUSY;
			goto out;
		}
	}

	/* Note old and new segment table endpoints, and old ifile size */
	olast = fs->lfs_cleansz + fs->lfs_segtabsz;
	nlast = howmany(newnsegs, fs->lfs_sepb) + fs->lfs_cleansz;
	ilast = ivp->v_size >> fs->lfs_bshift;
	noff = nlast - olast;

	/*
	 * Make sure no one can use the Ifile while we change it around.
	 * Even after taking the iflock we need to make sure no one still
	 * is holding Ifile buffers, so we get each one, to drain them.
	 * (XXX this could be done better.)
	 */
	rw_enter(&fs->lfs_iflock, RW_WRITER);
	vn_lock(ivp, LK_EXCLUSIVE | LK_RETRY);
	for (i = 0; i < ilast; i++) {
		bread(ivp, i, fs->lfs_bsize, NOCRED, 0, &bp);
		brelse(bp, 0);
	}

	/* Allocate new Ifile blocks */
	for (i = ilast; i < ilast + noff; i++) {
		if (lfs_balloc(ivp, i * fs->lfs_bsize, fs->lfs_bsize, NOCRED, 0,
			       &bp) != 0)
			panic("balloc extending ifile");
		memset(bp->b_data, 0, fs->lfs_bsize);
		VOP_BWRITE(bp->b_vp, bp);
	}

	/* Register new ifile size */
	ip->i_size += noff * fs->lfs_bsize; 
	ip->i_ffs1_size = ip->i_size;
	uvm_vnp_setsize(ivp, ip->i_size);

	/* Copy the inode table to its new position */
	if (noff != 0) {
		if (noff < 0) {
			start = nlast;
			end = ilast + noff;
			inc = 1;
		} else {
			start = ilast + noff - 1;
			end = nlast - 1;
			inc = -1;
		}
		for (i = start; i != end; i += inc) {
			if (bread(ivp, i, fs->lfs_bsize, NOCRED,
			    B_MODIFY, &bp) != 0)
				panic("resize: bread dst blk failed");
			if (bread(ivp, i - noff, fs->lfs_bsize,
			    NOCRED, 0, &obp))
				panic("resize: bread src blk failed");
			memcpy(bp->b_data, obp->b_data, fs->lfs_bsize);
			VOP_BWRITE(bp->b_vp, bp);
			brelse(obp, 0);
		}
	}

	/* If we are expanding, write the new empty SEGUSE entries */
	if (newnsegs > oldnsegs) {
		for (i = oldnsegs; i < newnsegs; i++) {
			if ((error = bread(ivp, i / fs->lfs_sepb +
					   fs->lfs_cleansz, fs->lfs_bsize,
					   NOCRED, B_MODIFY, &bp)) != 0)
				panic("lfs: ifile read: %d", error);
			while ((i + 1) % fs->lfs_sepb && i < newnsegs) {
				sup = &((SEGUSE *)bp->b_data)[i % fs->lfs_sepb];
				memset(sup, 0, sizeof(*sup));
				i++;
			}
			VOP_BWRITE(bp->b_vp, bp);
		}
	}

	/* Zero out unused superblock offsets */
	for (i = 2; i < LFS_MAXNUMSB; i++)
		if (dtosn(fs, fs->lfs_sboffs[i]) >= newnsegs)
			fs->lfs_sboffs[i] = 0x0;

	/*
	 * Correct superblock entries that depend on fs size.
	 * The computations of these are as follows:
	 *
	 * size  = segtod(fs, nseg)
	 * dsize = segtod(fs, nseg - minfreeseg) - btofsb(#super * LFS_SBPAD)
	 * bfree = dsize - btofsb(fs, bsize * nseg / 2) - blocks_actually_used
	 * avail = segtod(fs, nclean) - btofsb(#clean_super * LFS_SBPAD)
	 *         + (segtod(fs, 1) - (offset - curseg))
	 *	   - segtod(fs, minfreeseg - (minfreeseg / 2))
	 *
	 * XXX - we should probably adjust minfreeseg as well.
	 */
	gain = (newnsegs - oldnsegs);
	fs->lfs_nseg = newnsegs;
	fs->lfs_segtabsz = nlast - fs->lfs_cleansz;
	fs->lfs_size += gain * btofsb(fs, fs->lfs_ssize);
	fs->lfs_dsize += gain * btofsb(fs, fs->lfs_ssize) - btofsb(fs, sbbytes);
	fs->lfs_bfree += gain * btofsb(fs, fs->lfs_ssize) - btofsb(fs, sbbytes)
		       - gain * btofsb(fs, fs->lfs_bsize / 2);
	if (gain > 0) {
		fs->lfs_nclean += gain;
		fs->lfs_avail += gain * btofsb(fs, fs->lfs_ssize);
	} else {
		fs->lfs_nclean -= cgain;
		fs->lfs_avail -= cgain * btofsb(fs, fs->lfs_ssize) -
				 btofsb(fs, csbbytes);
	}

	/* Resize segment flag cache */
	fs->lfs_suflags[0] = (u_int32_t *)realloc(fs->lfs_suflags[0],
						  fs->lfs_nseg * sizeof(u_int32_t),
						  M_SEGMENT, M_WAITOK);
	fs->lfs_suflags[1] = (u_int32_t *)realloc(fs->lfs_suflags[1],
						  fs->lfs_nseg * sizeof(u_int32_t),
						  M_SEGMENT, M_WAITOK);
	for (i = oldnsegs; i < newnsegs; i++)
		fs->lfs_suflags[0][i] = fs->lfs_suflags[1][i] = 0x0;

	/* Truncate Ifile if necessary */
	if (noff < 0)
		lfs_truncate(ivp, ivp->v_size + (noff << fs->lfs_bshift), 0,
		    NOCRED);

	/* Update cleaner info so the cleaner can die */
	bread(ivp, 0, fs->lfs_bsize, NOCRED, B_MODIFY, &bp);
	((CLEANERINFO *)bp->b_data)->clean = fs->lfs_nclean;
	((CLEANERINFO *)bp->b_data)->dirty = fs->lfs_nseg - fs->lfs_nclean;
	VOP_BWRITE(bp->b_vp, bp);

	/* Let Ifile accesses proceed */
	VOP_UNLOCK(ivp);
	rw_exit(&fs->lfs_iflock);

    out:
	lfs_segunlock(fs);
	return error;
}
