/*	$NetBSD: nfs_vnops.c,v 1.308 2015/05/14 17:35:54 chs Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_vnops.c	8.19 (Berkeley) 7/31/95
 */

/*
 * vnode op calls for Sun NFS version 2 and 3
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_vnops.c,v 1.308 2015/05/14 17:35:54 chs Exp $");

#ifdef _KERNEL_OPT
#include "opt_nfs.h"
#include "opt_uvmhist.h"
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/condvar.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/hash.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/kauth.h>
#include <sys/cprng.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_var.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

/*
 * Global vfs data structures for nfs
 */
int (**nfsv2_vnodeop_p)(void *);
const struct vnodeopv_entry_desc nfsv2_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, nfs_lookup },		/* lookup */
	{ &vop_create_desc, nfs_create },		/* create */
	{ &vop_mknod_desc, nfs_mknod },			/* mknod */
	{ &vop_open_desc, nfs_open },			/* open */
	{ &vop_close_desc, nfs_close },			/* close */
	{ &vop_access_desc, nfs_access },		/* access */
	{ &vop_getattr_desc, nfs_getattr },		/* getattr */
	{ &vop_setattr_desc, nfs_setattr },		/* setattr */
	{ &vop_read_desc, nfs_read },			/* read */
	{ &vop_write_desc, nfs_write },			/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, nfs_ioctl },			/* ioctl */
	{ &vop_poll_desc, nfs_poll },			/* poll */
	{ &vop_kqfilter_desc, nfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, nfs_revoke },		/* revoke */
	{ &vop_mmap_desc, nfs_mmap },			/* mmap */
	{ &vop_fsync_desc, nfs_fsync },			/* fsync */
	{ &vop_seek_desc, nfs_seek },			/* seek */
	{ &vop_remove_desc, nfs_remove },		/* remove */
	{ &vop_link_desc, nfs_link },			/* link */
	{ &vop_rename_desc, nfs_rename },		/* rename */
	{ &vop_mkdir_desc, nfs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, nfs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, nfs_symlink },		/* symlink */
	{ &vop_readdir_desc, nfs_readdir },		/* readdir */
	{ &vop_readlink_desc, nfs_readlink },		/* readlink */
	{ &vop_abortop_desc, nfs_abortop },		/* abortop */
	{ &vop_inactive_desc, nfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, nfs_lock },			/* lock */
	{ &vop_unlock_desc, nfs_unlock },		/* unlock */
	{ &vop_bmap_desc, nfs_bmap },			/* bmap */
	{ &vop_strategy_desc, nfs_strategy },		/* strategy */
	{ &vop_print_desc, nfs_print },			/* print */
	{ &vop_islocked_desc, nfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, nfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, nfs_advlock },		/* advlock */
	{ &vop_bwrite_desc, genfs_badop },		/* bwrite */
	{ &vop_getpages_desc, nfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc nfsv2_vnodeop_opv_desc =
	{ &nfsv2_vnodeop_p, nfsv2_vnodeop_entries };

/*
 * Special device vnode ops
 */
int (**spec_nfsv2nodeop_p)(void *);
const struct vnodeopv_entry_desc spec_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, nfsspec_close },		/* close */
	{ &vop_access_desc, nfsspec_access },		/* access */
	{ &vop_getattr_desc, nfs_getattr },		/* getattr */
	{ &vop_setattr_desc, nfs_setattr },		/* setattr */
	{ &vop_read_desc, nfsspec_read },		/* read */
	{ &vop_write_desc, nfsspec_write },		/* write */
	{ &vop_fallocate_desc, spec_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, spec_fdiscard },		/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
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
	{ &vop_inactive_desc, nfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, nfs_lock },			/* lock */
	{ &vop_unlock_desc, nfs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, nfs_print },			/* print */
	{ &vop_islocked_desc, nfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, spec_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc spec_nfsv2nodeop_opv_desc =
	{ &spec_nfsv2nodeop_p, spec_nfsv2nodeop_entries };

int (**fifo_nfsv2nodeop_p)(void *);
const struct vnodeopv_entry_desc fifo_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup */
	{ &vop_create_desc, vn_fifo_bypass },		/* create */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* mknod */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, nfsfifo_close },		/* close */
	{ &vop_access_desc, nfsspec_access },		/* access */
	{ &vop_getattr_desc, nfs_getattr },		/* getattr */
	{ &vop_setattr_desc, nfs_setattr },		/* setattr */
	{ &vop_read_desc, nfsfifo_read },		/* read */
	{ &vop_write_desc, nfsfifo_write },		/* write */
	{ &vop_fallocate_desc, vn_fifo_bypass },	/* fallocate */
	{ &vop_fdiscard_desc, vn_fifo_bypass },		/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* mmap */
	{ &vop_fsync_desc, nfs_fsync },			/* fsync */
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
	{ &vop_inactive_desc, nfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, nfs_lock },			/* lock */
	{ &vop_unlock_desc, nfs_unlock },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, genfs_badop },		/* strategy */
	{ &vop_print_desc, nfs_print },			/* print */
	{ &vop_islocked_desc, nfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, genfs_badop },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass }, 	/* putpages */
	{ NULL, NULL }
};
const struct vnodeopv_desc fifo_nfsv2nodeop_opv_desc =
	{ &fifo_nfsv2nodeop_p, fifo_nfsv2nodeop_entries };

static int nfs_linkrpc(struct vnode *, struct vnode *, const char *,
    size_t, kauth_cred_t, struct lwp *);
static void nfs_writerpc_extfree(struct mbuf *, void *, size_t, void *);

/*
 * Global variables
 */
extern u_int32_t nfs_true, nfs_false;
extern u_int32_t nfs_xdrneg1;
extern const nfstype nfsv3_type[9];

int nfs_numasync = 0;
#define	DIRHDSIZ	_DIRENT_NAMEOFF(dp)
#define UIO_ADVANCE(uio, siz) \
    (void)((uio)->uio_resid -= (siz), \
    (uio)->uio_iov->iov_base = (char *)(uio)->uio_iov->iov_base + (siz), \
    (uio)->uio_iov->iov_len -= (siz))

static void nfs_cache_enter(struct vnode *, struct vnode *,
    struct componentname *);

static void
nfs_cache_enter(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp)
{
	struct nfsnode *dnp = VTONFS(dvp);

	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		return;
	}
	if (vp != NULL) {
		struct nfsnode *np = VTONFS(vp);

		np->n_ctime = np->n_vattr->va_ctime.tv_sec;
	}

	if (!timespecisset(&dnp->n_nctime))
		dnp->n_nctime = dnp->n_vattr->va_mtime;

	cache_enter(dvp, vp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_flags);
}

/*
 * nfs null call from vfs.
 */
int
nfs_null(struct vnode *vp, kauth_cred_t cred, struct lwp *l)
{
	char *bpos, *dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb __unused;
	struct nfsnode *np = VTONFS(vp);

	nfsm_reqhead(np, NFSPROC_NULL, 0);
	nfsm_request(np, NFSPROC_NULL, l, cred);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs access vnode op.
 * For nfs version 2, just return ok. File accesses may fail later.
 * For nfs version 3, use the access rpc to check accessibility. If file modes
 * are changed on the server, accesses might still fail later.
 */
int
nfs_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
#ifndef NFS_V2_ONLY
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	int error = 0, attrflag;
	struct mbuf *mreq, *mrep, *md, *mb;
	u_int32_t mode, rmode;
	const int v3 = NFS_ISV3(vp);
#endif
	int cachevalid;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	cachevalid = (np->n_accstamp != -1 &&
	    (time_uptime - np->n_accstamp) < nfs_attrtimeo(nmp, np) &&
	    np->n_accuid == kauth_cred_geteuid(ap->a_cred));

	/*
	 * Check access cache first. If this request has been made for this
	 * uid shortly before, use the cached result.
	 */
	if (cachevalid) {
		if (!np->n_accerror) {
			if  ((np->n_accmode & ap->a_mode) == ap->a_mode)
				return np->n_accerror;
		} else if ((np->n_accmode & ap->a_mode) == np->n_accmode)
			return np->n_accerror;
	}

#ifndef NFS_V2_ONLY
	/*
	 * For nfs v3, do an access rpc, otherwise you are stuck emulating
	 * ufs_access() locally using the vattr. This may not be correct,
	 * since the server may apply other access criteria such as
	 * client uid-->server uid mapping that we do not know about, but
	 * this is better than just returning anything that is lying about
	 * in the cache.
	 */
	if (v3) {
		nfsstats.rpccnt[NFSPROC_ACCESS]++;
		nfsm_reqhead(np, NFSPROC_ACCESS, NFSX_FH(v3) + NFSX_UNSIGNED);
		nfsm_fhtom(np, v3);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		if (ap->a_mode & VREAD)
			mode = NFSV3ACCESS_READ;
		else
			mode = 0;
		if (vp->v_type != VDIR) {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_EXECUTE;
		} else {
			if (ap->a_mode & VWRITE)
				mode |= (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
					 NFSV3ACCESS_DELETE);
			if (ap->a_mode & VEXEC)
				mode |= NFSV3ACCESS_LOOKUP;
		}
		*tl = txdr_unsigned(mode);
		nfsm_request(np, NFSPROC_ACCESS, curlwp, ap->a_cred);
		nfsm_postop_attr(vp, attrflag, 0);
		if (!error) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			rmode = fxdr_unsigned(u_int32_t, *tl);
			/*
			 * The NFS V3 spec does not clarify whether or not
			 * the returned access bits can be a superset of
			 * the ones requested, so...
			 */
			if ((rmode & mode) != mode)
				error = EACCES;
		}
		nfsm_reqdone;
	} else
#endif
		return (nfsspec_access(ap));
#ifndef NFS_V2_ONLY
	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if (!error && (ap->a_mode & VWRITE) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			error = EROFS;
		default:
			break;
		}
	}

	if (!error || error == EACCES) {
		/*
		 * If we got the same result as for a previous,
		 * different request, OR it in. Don't update
		 * the timestamp in that case.
		 */
		if (cachevalid && np->n_accstamp != -1 &&
		    error == np->n_accerror) {
			if (!error)
				np->n_accmode |= ap->a_mode;
			else if ((np->n_accmode & ap->a_mode) == ap->a_mode)
				np->n_accmode = ap->a_mode;
		} else {
			np->n_accstamp = time_uptime;
			np->n_accuid = kauth_cred_geteuid(ap->a_cred);
			np->n_accmode = ap->a_mode;
			np->n_accerror = error;
		}
	}

	return (error);
#endif
}

/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
/* ARGSUSED */
int
nfs_open(void *v)
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	int error;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK) {
		return (EACCES);
	}

	if (ap->a_mode & FREAD) {
		if (np->n_rcred != NULL)
			kauth_cred_free(np->n_rcred);
		np->n_rcred = ap->a_cred;
		kauth_cred_hold(np->n_rcred);
	}
	if (ap->a_mode & FWRITE) {
		if (np->n_wcred != NULL)
			kauth_cred_free(np->n_wcred);
		np->n_wcred = ap->a_cred;
		kauth_cred_hold(np->n_wcred);
	}

	error = nfs_flushstalebuf(vp, ap->a_cred, curlwp, 0);
	if (error)
		return error;

	NFS_INVALIDATE_ATTRCACHE(np); /* For Open/Close consistency */

	return (0);
}

/*
 * nfs close vnode op
 * What an NFS client should do upon close after writing is a debatable issue.
 * Most NFS clients push delayed writes to the server upon close, basically for
 * two reasons:
 * 1 - So that any write errors may be reported back to the client process
 *     doing the close system call. By far the two most likely errors are
 *     NFSERR_NOSPC and NFSERR_DQUOT to indicate space allocation failure.
 * 2 - To put a worst case upper bound on cache inconsistency between
 *     multiple clients for the file.
 * There is also a consistency problem for Version 2 of the protocol w.r.t.
 * not being able to tell if other clients are writing a file concurrently,
 * since there is no way of knowing if the changed modify time in the reply
 * is only due to the write for this client.
 * (NFS Version 3 provides weak cache consistency data in the reply that
 *  should be sufficient to detect and handle this case.)
 *
 * The current code does the following:
 * for NFS Version 2 - play it safe and flush/invalidate all dirty buffers
 * for NFS Version 3 - flush dirty buffers to the server but don't invalidate
 *                     or commit them (this satisfies 1 and 2 except for the
 *                     case where the server crashes after this close but
 *                     before the commit RPC, which is felt to be "good
 *                     enough". Changing the last argument to nfs_flush() to
 *                     a 1 would force a commit operation, if it is felt a
 *                     commit is necessary now.
 */
/* ARGSUSED */
int
nfs_close(void *v)
{
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	UVMHIST_FUNC("nfs_close"); UVMHIST_CALLED(ubchist);

	if (vp->v_type == VREG) {
	    if (np->n_flag & NMODIFIED) {
#ifndef NFS_V2_ONLY
		if (NFS_ISV3(vp)) {
		    error = nfs_flush(vp, ap->a_cred, MNT_WAIT, curlwp, 0);
		    np->n_flag &= ~NMODIFIED;
		} else
#endif
		    error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, curlwp, 1);
		NFS_INVALIDATE_ATTRCACHE(np);
	    }
	    if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		error = np->n_error;
	    }
	}
	UVMHIST_LOG(ubchist, "returning %d", error,0,0,0);
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
int
nfs_getattr(void *v)
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	char *cp;
	u_int32_t *tl;
	int32_t t1, t2;
	char *bpos, *dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(vp);

	/*
	 * Update local times for special files.
	 */
	if (np->n_flag & (NACC | NUPD))
		np->n_flag |= NCHG;

	/*
	 * if we have delayed truncation, do it now.
	 */
	nfs_delayedtruncate(vp);

	/*
	 * First look in the cache.
	 */
	if (nfs_getattrcache(vp, ap->a_vap) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_GETATTR]++;
	nfsm_reqhead(np, NFSPROC_GETATTR, NFSX_FH(v3));
	nfsm_fhtom(np, v3);
	nfsm_request(np, NFSPROC_GETATTR, curlwp, ap->a_cred);
	if (!error) {
		nfsm_loadattr(vp, ap->a_vap, 0);
		if (vp->v_type == VDIR &&
		    ap->a_vap->va_blocksize < NFS_DIRFRAGSIZ)
			ap->a_vap->va_blocksize = NFS_DIRFRAGSIZ;
	}
	nfsm_reqdone;
	return (error);
}

/*
 * nfs setattr call.
 */
int
nfs_setattr(void *v)
{
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr *vap = ap->a_vap;
	int error = 0;
	u_quad_t tsize = 0;

	/*
	 * Setting of flags is not supported.
	 */
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
		if (vap->va_size > VFSTONFS(vp->v_mount)->nm_maxfilesize) {
			return EFBIG;
		}
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			if (vap->va_mtime.tv_sec == VNOVAL &&
			    vap->va_atime.tv_sec == VNOVAL &&
			    vap->va_mode == (mode_t)VNOVAL &&
			    vap->va_uid == (uid_t)VNOVAL &&
			    vap->va_gid == (gid_t)VNOVAL)
				return (0);
 			vap->va_size = VNOVAL;
 			break;
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			genfs_node_wrlock(vp);
 			uvm_vnp_setsize(vp, vap->va_size);
 			tsize = np->n_size;
			np->n_size = vap->va_size;
 			if (vap->va_size == 0)
 				error = nfs_vinvalbuf(vp, 0,
 				     ap->a_cred, curlwp, 1);
			else
				error = nfs_vinvalbuf(vp, V_SAVE,
				     ap->a_cred, curlwp, 1);
			if (error) {
				uvm_vnp_setsize(vp, tsize);
				genfs_node_unlock(vp);
				return (error);
			}
 			np->n_vattr->va_size = vap->va_size;
  		}
  	} else {
		/*
		 * flush files before setattr because a later write of
		 * cached data might change timestamps or reset sugid bits
		 */
		if ((vap->va_mtime.tv_sec != VNOVAL ||
		     vap->va_atime.tv_sec != VNOVAL ||
		     vap->va_mode != VNOVAL) &&
		    vp->v_type == VREG &&
  		    (error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
		 			   curlwp, 1)) == EINTR)
			return (error);
	}
	error = nfs_setattrrpc(vp, vap, ap->a_cred, curlwp);
	if (vap->va_size != VNOVAL) {
		if (error) {
			np->n_size = np->n_vattr->va_size = tsize;
			uvm_vnp_setsize(vp, np->n_size);
		}
		genfs_node_unlock(vp);
	}
	VN_KNOTE(vp, NOTE_ATTRIB);
	return (error);
}

/*
 * Do an nfs setattr rpc.
 */
int
nfs_setattrrpc(struct vnode *vp, struct vattr *vap, kauth_cred_t cred, struct lwp *l)
{
	struct nfsv2_sattr *sp;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos;
	u_int32_t *tl;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(vp);
	struct nfsnode *np = VTONFS(vp);
#ifndef NFS_V2_ONLY
	int wccflag = NFSV3_WCCRATTR;
	char *cp2;
#endif

	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	nfsm_reqhead(np, NFSPROC_SETATTR, NFSX_FH(v3) + NFSX_SATTR(v3));
	nfsm_fhtom(np, v3);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_v3attrbuild(vap, true);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl = nfs_false;
	} else {
#endif
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		if (vap->va_mode == (mode_t)VNOVAL)
			sp->sa_mode = nfs_xdrneg1;
		else
			sp->sa_mode = vtonfsv2_mode(vp->v_type, vap->va_mode);
		if (vap->va_uid == (uid_t)VNOVAL)
			sp->sa_uid = nfs_xdrneg1;
		else
			sp->sa_uid = txdr_unsigned(vap->va_uid);
		if (vap->va_gid == (gid_t)VNOVAL)
			sp->sa_gid = nfs_xdrneg1;
		else
			sp->sa_gid = txdr_unsigned(vap->va_gid);
		sp->sa_size = txdr_unsigned(vap->va_size);
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
#ifndef NFS_V2_ONLY
	}
#endif
	nfsm_request(np, NFSPROC_SETATTR, l, cred);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_wcc_data(vp, wccflag, NAC_NOTRUNC, false);
	} else
#endif
		nfsm_loadattr(vp, (struct vattr *)0, NAC_NOTRUNC);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, do the rpc.
 */
int
nfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	int flags;
	struct vnode *newvp;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	struct mbuf *mreq, *mrep, *md, *mb;
	long len;
	nfsfh_t *fhp;
	struct nfsnode *np;
	int cachefound;
	int error = 0, attrflag, fhsize;
	const int v3 = NFS_ISV3(dvp);

	flags = cnp->cn_flags;

	*vpp = NULLVP;
	newvp = NULLVP;
	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * RFC1813(nfsv3) 3.2 says clients should handle "." by themselves.
	 */
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
		if (error)
			return error;
		if (cnp->cn_nameiop == RENAME && (flags & ISLASTCN))
			return EISDIR;
		vref(dvp);
		*vpp = dvp;
		return 0;
	}

	np = VTONFS(dvp);

	/*
	 * Before performing an RPC, check the name cache to see if
	 * the directory/name pair we are looking for is known already.
	 * If the directory/name pair is found in the name cache,
	 * we have to ensure the directory has not changed from
	 * the time the cache entry has been created. If it has,
	 * the cache entry has to be ignored.
	 */
	cachefound = cache_lookup_raw(dvp, cnp->cn_nameptr, cnp->cn_namelen,
				      cnp->cn_flags, NULL, vpp);
	KASSERT(dvp != *vpp);
	KASSERT((cnp->cn_flags & ISWHITEOUT) == 0);
	if (cachefound) {
		struct vattr vattr;

		error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred);
		if (error != 0) {
			if (*vpp != NULLVP)
				vrele(*vpp);
			*vpp = NULLVP;
			return error;
		}

		if (VOP_GETATTR(dvp, &vattr, cnp->cn_cred)
		    || timespeccmp(&vattr.va_mtime,
		    &VTONFS(dvp)->n_nctime, !=)) {
			if (*vpp != NULLVP) {
				vrele(*vpp);
				*vpp = NULLVP;
			}
			cache_purge1(dvp, NULL, 0, PURGE_CHILDREN);
			timespecclear(&np->n_nctime);
			goto dorpc;
		}

		if (*vpp == NULLVP) {
			/* namecache gave us a negative result */
			error = ENOENT;
			goto noentry;
		}

		/*
		 * investigate the vnode returned by cache_lookup_raw.
		 * if it isn't appropriate, do an rpc.
		 */
		newvp = *vpp;
		if ((flags & ISDOTDOT) != 0) {
			VOP_UNLOCK(dvp);
		}
		error = vn_lock(newvp, LK_SHARED);
		if ((flags & ISDOTDOT) != 0) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		}
		if (error != 0) {
			/* newvp has been reclaimed. */
			vrele(newvp);
			*vpp = NULLVP;
			goto dorpc;
		}
		if (!VOP_GETATTR(newvp, &vattr, cnp->cn_cred)
		    && vattr.va_ctime.tv_sec == VTONFS(newvp)->n_ctime) {
			nfsstats.lookupcache_hits++;
			KASSERT(newvp->v_type != VNON);
			VOP_UNLOCK(newvp);
			return (0);
		}
		cache_purge1(newvp, NULL, 0, PURGE_PARENTS);
		vput(newvp);
		*vpp = NULLVP;
	}
dorpc:
#if 0
	/*
	 * because nfsv3 has the same CREATE semantics as ours,
	 * we don't have to perform LOOKUPs beforehand.
	 *
	 * XXX ideally we can do the same for nfsv2 in the case of !O_EXCL.
	 * XXX although we have no way to know if O_EXCL is requested or not.
	 */

	if (v3 && cnp->cn_nameiop == CREATE &&
	    (flags & (ISLASTCN|ISDOTDOT)) == ISLASTCN &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		return (EJUSTRETURN);
	}
#endif /* 0 */

	error = 0;
	newvp = NULLVP;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = cnp->cn_namelen;
	nfsm_reqhead(np, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(np, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	nfsm_request(np, NFSPROC_LOOKUP, curlwp, cnp->cn_cred);
	if (error) {
		nfsm_postop_attr(dvp, attrflag, 0);
		m_freem(mrep);
		goto nfsmout;
	}
	nfsm_getfh(fhp, fhsize, v3);

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && (flags & ISLASTCN)) {
		if (NFS_CMPFH(np, fhp, fhsize)) {
			m_freem(mrep);
			return (EISDIR);
		}
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(mrep);
			return error;
		}
		newvp = NFSTOV(np);
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(newvp, attrflag, 0);
			nfsm_postop_attr(dvp, attrflag, 0);
		} else
#endif
			nfsm_loadattr(newvp, (struct vattr *)0, 0);
		*vpp = newvp;
		m_freem(mrep);
		goto validate;
	}

	/*
	 * The postop attr handling is duplicated for each if case,
	 * because it should be done while dvp is locked (unlocking
	 * dvp is different for each case).
	 */

	if (NFS_CMPFH(np, fhp, fhsize)) {
		/*
		 * as we handle "." lookup locally, this should be
		 * a broken server.
		 */
		vref(dvp);
		newvp = dvp;
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(newvp, attrflag, 0);
			nfsm_postop_attr(dvp, attrflag, 0);
		} else
#endif
			nfsm_loadattr(newvp, (struct vattr *)0, 0);
	} else if (flags & ISDOTDOT) {
		/*
		 * ".." lookup
		 */
		VOP_UNLOCK(dvp);
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		if (error) {
			m_freem(mrep);
			return error;
		}
		newvp = NFSTOV(np);

#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(newvp, attrflag, 0);
			nfsm_postop_attr(dvp, attrflag, 0);
		} else
#endif
			nfsm_loadattr(newvp, (struct vattr *)0, 0);
	} else {
		/*
		 * Other lookups.
		 */
		error = nfs_nget(dvp->v_mount, fhp, fhsize, &np);
		if (error) {
			m_freem(mrep);
			return error;
		}
		newvp = NFSTOV(np);
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(newvp, attrflag, 0);
			nfsm_postop_attr(dvp, attrflag, 0);
		} else
#endif
			nfsm_loadattr(newvp, (struct vattr *)0, 0);
	}
	if (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN)) {
		nfs_cache_enter(dvp, newvp, cnp);
	}
	*vpp = newvp;
	nfsm_reqdone;
	if (error) {
		/*
		 * We get here only because of errors returned by
		 * the RPC. Otherwise we'll have returned above
		 * (the nfsm_* macros will jump to nfsm_reqdone
		 * on error).
		 */
		if (error == ENOENT && cnp->cn_nameiop != CREATE) {
			nfs_cache_enter(dvp, NULL, cnp);
		}
		if (newvp != NULLVP) {
			if (newvp == dvp) {
				vrele(newvp);
			} else {
				vput(newvp);
			}
		}
noentry:
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN) && error == ENOENT) {
			if (dvp->v_mount->mnt_flag & MNT_RDONLY) {
				error = EROFS;
			} else {
				error = EJUSTRETURN;
			}
		}
		*vpp = NULL;
		return error;
	}

validate:
	/*
	 * make sure we have valid type and size.
	 */

	newvp = *vpp;
	if (newvp->v_type == VNON) {
		struct vattr vattr; /* dummy */

		KASSERT(VTONFS(newvp)->n_attrstamp == 0);
		error = VOP_GETATTR(newvp, &vattr, cnp->cn_cred);
		if (error) {
			vput(newvp);
			*vpp = NULL;
		}
	}
	if (error)
		return error;
	if (newvp != dvp)
		VOP_UNLOCK(newvp);
	return 0;
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
int
nfs_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VREG)
		return EISDIR;
	return (nfs_bioread(vp, ap->a_uio, ap->a_ioflag, ap->a_cred, 0));
}

/*
 * nfs readlink call
 */
int
nfs_readlink(void *v)
{
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	if (vp->v_type != VLNK)
		return (EPERM);

	if (np->n_rcred != NULL) {
		kauth_cred_free(np->n_rcred);
	}
	np->n_rcred = ap->a_cred;
	kauth_cred_hold(np->n_rcred);

	return (nfs_bioread(vp, ap->a_uio, 0, ap->a_cred, 0));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
int
nfs_readlinkrpc(struct vnode *vp, struct uio *uiop, kauth_cred_t cred)
{
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	int error = 0;
	uint32_t len;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(vp);
	struct nfsnode *np = VTONFS(vp);
#ifndef NFS_V2_ONLY
	int attrflag;
#endif

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	nfsm_reqhead(np, NFSPROC_READLINK, NFSX_FH(v3));
	nfsm_fhtom(np, v3);
	nfsm_request(np, NFSPROC_READLINK, curlwp, cred);
#ifndef NFS_V2_ONLY
	if (v3)
		nfsm_postop_attr(vp, attrflag, 0);
#endif
	if (!error) {
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_dissect(tl, uint32_t *, NFSX_UNSIGNED);
			len = fxdr_unsigned(uint32_t, *tl);
			if (len > NFS_MAXPATHLEN) {
				/*
				 * this pathname is too long for us.
				 */
				m_freem(mrep);
				/* Solaris returns EINVAL. should we follow? */
				error = ENAMETOOLONG;
				goto nfsmout;
			}
		} else
#endif
		{
			nfsm_strsiz(len, NFS_MAXPATHLEN);
		}
		nfsm_mtouio(uiop, len);
	}
	nfsm_reqdone;
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs_readrpc(struct vnode *vp, struct uio *uiop)
{
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsmount *nmp;
	int error = 0, len, retlen, tsiz, eof __unused, byte_count;
	const int v3 = NFS_ISV3(vp);
	struct nfsnode *np = VTONFS(vp);
#ifndef NFS_V2_ONLY
	int attrflag;
#endif

#ifndef nolint
	eof = 0;
#endif
	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > nmp->nm_maxfilesize)
		return (EFBIG);
	iostat_busy(nmp->nm_stats);
	byte_count = 0; /* count bytes actually transferred */
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		nfsm_reqhead(np, NFSPROC_READ, NFSX_FH(v3) + NFSX_UNSIGNED * 3);
		nfsm_fhtom(np, v3);
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED * 3);
#ifndef NFS_V2_ONLY
		if (v3) {
			txdr_hyper(uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		} else
#endif
		{
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		}
		nfsm_request(np, NFSPROC_READ, curlwp, np->n_rcred);
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(vp, attrflag, NAC_NOTRUNC);
			if (error) {
				m_freem(mrep);
				goto nfsmout;
			}
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			eof = fxdr_unsigned(int, *(tl + 1));
		} else
#endif
			nfsm_loadattr(vp, (struct vattr *)0, NAC_NOTRUNC);
		nfsm_strsiz(retlen, nmp->nm_rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(mrep);
		tsiz -= retlen;
		byte_count += retlen;
#ifndef NFS_V2_ONLY
		if (v3) {
			if (eof || retlen == 0)
				tsiz = 0;
		} else
#endif
		if (retlen < len)
			tsiz = 0;
	}
nfsmout:
	iostat_unbusy(nmp->nm_stats, byte_count, 1);
	return (error);
}

struct nfs_writerpc_context {
	kmutex_t nwc_lock;
	kcondvar_t nwc_cv;
	int nwc_mbufcount;
};

/*
 * free mbuf used to refer protected pages while write rpc call.
 * called at splvm.
 */
static void
nfs_writerpc_extfree(struct mbuf *m, void *tbuf, size_t size, void *arg)
{
	struct nfs_writerpc_context *ctx = arg;

	KASSERT(m != NULL);
	KASSERT(ctx != NULL);
	pool_cache_put(mb_cache, m);
	mutex_enter(&ctx->nwc_lock);
	if (--ctx->nwc_mbufcount == 0) {
		cv_signal(&ctx->nwc_cv);
	}
	mutex_exit(&ctx->nwc_lock);
}

/*
 * nfs write call
 */
int
nfs_writerpc(struct vnode *vp, struct uio *uiop, int *iomode, bool pageprotected, bool *stalewriteverfp)
{
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, len, tsiz, wccflag = NFSV3_WCCRATTR;
	const int v3 = NFS_ISV3(vp);
	int committed = NFSV3WRITE_FILESYNC;
	struct nfsnode *np = VTONFS(vp);
	struct nfs_writerpc_context ctx;
	int byte_count;
	size_t origresid;
#ifndef NFS_V2_ONLY
	char *cp2;
	int rlen, commit;
#endif

	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		panic("writerpc readonly vp %p", vp);
	}

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1)
		panic("nfs: writerpc iovcnt > 1");
#endif
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > nmp->nm_maxfilesize)
		return EFBIG;

	mutex_init(&ctx.nwc_lock, MUTEX_DRIVER, IPL_VM);
	cv_init(&ctx.nwc_cv, "nfsmblk");
	ctx.nwc_mbufcount = 1;

retry:
	origresid = uiop->uio_resid;
	KASSERT(origresid == uiop->uio_iov->iov_len);
	iostat_busy(nmp->nm_stats);
	byte_count = 0; /* count of bytes actually written */
	while (tsiz > 0) {
		uint32_t datalen; /* data bytes need to be allocated in mbuf */
		size_t backup;
		bool stalewriteverf = false;

		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = min(tsiz, nmp->nm_wsize);
		datalen = pageprotected ? 0 : nfsm_rndup(len);
		nfsm_reqhead(np, NFSPROC_WRITE,
			NFSX_FH(v3) + 5 * NFSX_UNSIGNED + datalen);
		nfsm_fhtom(np, v3);
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_build(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			txdr_hyper(uiop->uio_offset, tl);
			tl += 2;
			*tl++ = txdr_unsigned(len);
			*tl++ = txdr_unsigned(*iomode);
			*tl = txdr_unsigned(len);
		} else
#endif
		{
			u_int32_t x;

			nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			/* Set both "begin" and "current" to non-garbage. */
			x = txdr_unsigned((u_int32_t)uiop->uio_offset);
			*tl++ = x;      /* "begin offset" */
			*tl++ = x;      /* "current offset" */
			x = txdr_unsigned(len);
			*tl++ = x;      /* total to this offset */
			*tl = x;        /* size of this write */

		}
		if (pageprotected) {
			/*
			 * since we know pages can't be modified during i/o,
			 * no need to copy them for us.
			 */
			struct mbuf *m;
			struct iovec *iovp = uiop->uio_iov;

			m = m_get(M_WAIT, MT_DATA);
			MCLAIM(m, &nfs_mowner);
			MEXTADD(m, iovp->iov_base, len, M_MBUF,
			    nfs_writerpc_extfree, &ctx);
			m->m_flags |= M_EXT_ROMAP;
			m->m_len = len;
			mb->m_next = m;
			/*
			 * no need to maintain mb and bpos here
			 * because no one care them later.
			 */
#if 0
			mb = m;
			bpos = mtod(void *, mb) + mb->m_len;
#endif
			UIO_ADVANCE(uiop, len);
			uiop->uio_offset += len;
			mutex_enter(&ctx.nwc_lock);
			ctx.nwc_mbufcount++;
			mutex_exit(&ctx.nwc_lock);
			nfs_zeropad(mb, 0, nfsm_padlen(len));
		} else {
			nfsm_uiotom(uiop, len);
		}
		nfsm_request(np, NFSPROC_WRITE, curlwp, np->n_wcred);
#ifndef NFS_V2_ONLY
		if (v3) {
			wccflag = NFSV3_WCCCHK;
			nfsm_wcc_data(vp, wccflag, NAC_NOTRUNC, !error);
			if (!error) {
				nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED
					+ NFSX_V3WRITEVERF);
				rlen = fxdr_unsigned(int, *tl++);
				if (rlen == 0) {
					error = NFSERR_IO;
					m_freem(mrep);
					break;
				} else if (rlen < len) {
					backup = len - rlen;
					UIO_ADVANCE(uiop, -backup);
					uiop->uio_offset -= backup;
					len = rlen;
				}
				commit = fxdr_unsigned(int, *tl++);

				/*
				 * Return the lowest committment level
				 * obtained by any of the RPCs.
				 */
				if (committed == NFSV3WRITE_FILESYNC)
					committed = commit;
				else if (committed == NFSV3WRITE_DATASYNC &&
					commit == NFSV3WRITE_UNSTABLE)
					committed = commit;
				mutex_enter(&nmp->nm_lock);
				if ((nmp->nm_iflag & NFSMNT_HASWRITEVERF) == 0){
					memcpy(nmp->nm_writeverf, tl,
					    NFSX_V3WRITEVERF);
					nmp->nm_iflag |= NFSMNT_HASWRITEVERF;
				} else if ((nmp->nm_iflag &
				    NFSMNT_STALEWRITEVERF) ||
				    memcmp(tl, nmp->nm_writeverf,
				    NFSX_V3WRITEVERF)) {
					memcpy(nmp->nm_writeverf, tl,
					    NFSX_V3WRITEVERF);
					/*
					 * note NFSMNT_STALEWRITEVERF
					 * if we're the first thread to
					 * notice it.
					 */
					if ((nmp->nm_iflag &
					    NFSMNT_STALEWRITEVERF) == 0) {
						stalewriteverf = true;
						nmp->nm_iflag |=
						    NFSMNT_STALEWRITEVERF;
					}
				}
				mutex_exit(&nmp->nm_lock);
			}
		} else
#endif
			nfsm_loadattr(vp, (struct vattr *)0, NAC_NOTRUNC);
		if (wccflag)
			VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr->va_mtime;
		m_freem(mrep);
		if (error)
			break;
		tsiz -= len;
		byte_count += len;
		if (stalewriteverf) {
			*stalewriteverfp = true;
			stalewriteverf = false;
			if (committed == NFSV3WRITE_UNSTABLE &&
			    len != origresid) {
				/*
				 * if our write requests weren't atomic but
				 * unstable, datas in previous iterations
				 * might have already been lost now.
				 * then, we should resend them to nfsd.
				 */
				backup = origresid - tsiz;
				UIO_ADVANCE(uiop, -backup);
				uiop->uio_offset -= backup;
				tsiz = origresid;
				goto retry;
			}
		}
	}
nfsmout:
	iostat_unbusy(nmp->nm_stats, byte_count, 0);
	if (pageprotected) {
		/*
		 * wait until mbufs go away.
		 * retransmitted mbufs can survive longer than rpc requests
		 * themselves.
		 */
		mutex_enter(&ctx.nwc_lock);
		ctx.nwc_mbufcount--;
		while (ctx.nwc_mbufcount > 0) {
			cv_wait(&ctx.nwc_cv, &ctx.nwc_lock);
		}
		mutex_exit(&ctx.nwc_lock);
	}
	mutex_destroy(&ctx.nwc_lock);
	cv_destroy(&ctx.nwc_cv);
	*iomode = committed;
	if (error)
		uiop->uio_resid = tsiz;
	return (error);
}

/*
 * nfs mknod rpc
 * For NFS v2 this is a kludge. Use a create rpc but with the IFMT bits of the
 * mode set to specify the file type and the size field for rdev.
 */
int
nfs_mknodrpc(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp, struct vattr *vap)
{
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	struct vnode *newvp = (struct vnode *)0;
	struct nfsnode *dnp, *np;
	char *cp2;
	char *bpos, *dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	u_int32_t rdev;
	const int v3 = NFS_ISV3(dvp);

	if (vap->va_type == VCHR || vap->va_type == VBLK)
		rdev = txdr_unsigned(vap->va_rdev);
	else if (vap->va_type == VFIFO || vap->va_type == VSOCK)
		rdev = nfs_xdrneg1;
	else {
		VOP_ABORTOP(dvp, cnp);
		return (EOPNOTSUPP);
	}
	nfsstats.rpccnt[NFSPROC_MKNOD]++;
	dnp = VTONFS(dvp);
	nfsm_reqhead(dnp, NFSPROC_MKNOD, NFSX_FH(v3) + 4 * NFSX_UNSIGNED +
		+ nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		*tl++ = vtonfsv3_type(vap->va_type);
		nfsm_v3attrbuild(vap, false);
		if (vap->va_type == VCHR || vap->va_type == VBLK) {
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(major(vap->va_rdev));
			*tl = txdr_unsigned(minor(vap->va_rdev));
		}
	} else
#endif
	{
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = rdev;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dnp, NFSPROC_MKNOD, curlwp, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, curlwp, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
#ifndef NFS_V2_ONLY
	if (v3)
		nfsm_wcc_data(dvp, wccflag, 0, !error);
#endif
	nfsm_reqdone;
	if (error) {
		if (newvp)
			vput(newvp);
	} else {
		nfs_cache_enter(dvp, newvp, cnp);
		*vpp = newvp;
		VOP_UNLOCK(newvp);
	}
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	return (error);
}

/*
 * nfs mknod vop
 * just call nfs_mknodrpc() to do the work.
 */
/* ARGSUSED */
int
nfs_mknod(void *v)
{
	struct vop_mknod_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	int error;

	error = nfs_mknodrpc(dvp, ap->a_vpp, cnp, ap->a_vap);
	VN_KNOTE(dvp, NOTE_WRITE);
	if (error == 0 || error == EEXIST)
		cache_purge1(dvp, cnp->cn_nameptr, cnp->cn_namelen, 0);
	return (error);
}

/*
 * nfs file create call
 */
int
nfs_create(void *v)
{
	struct vop_create_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	struct nfsnode *dnp, *np = (struct nfsnode *)0;
	struct vnode *newvp = (struct vnode *)0;
	char *bpos, *dpos, *cp2;
	int error, wccflag = NFSV3_WCCRATTR, gotvp = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(dvp);
	u_int32_t excl_mode = NFSV3CREATE_UNCHECKED;

	/*
	 * Oops, not for me..
	 */
	if (vap->va_type == VSOCK)
		return (nfs_mknodrpc(dvp, ap->a_vpp, cnp, vap));

	KASSERT(vap->va_type == VREG);

#ifdef VA_EXCLUSIVE
	if (vap->va_vaflags & VA_EXCLUSIVE) {
		excl_mode = NFSV3CREATE_EXCLUSIVE;
	}
#endif
again:
	error = 0;
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	dnp = VTONFS(dvp);
	nfsm_reqhead(dnp, NFSPROC_CREATE, NFSX_FH(v3) + 2 * NFSX_UNSIGNED +
		nfsm_rndup(cnp->cn_namelen) + NFSX_SATTR(v3));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
		if (excl_mode == NFSV3CREATE_EXCLUSIVE) {
			*tl = txdr_unsigned(NFSV3CREATE_EXCLUSIVE);
			nfsm_build(tl, u_int32_t *, NFSX_V3CREATEVERF);
			*tl++ = cprng_fast32();
			*tl = cprng_fast32();
		} else {
			*tl = txdr_unsigned(excl_mode);
			nfsm_v3attrbuild(vap, false);
		}
	} else
#endif
	{
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(vap->va_type, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = 0;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request(dnp, NFSPROC_CREATE, curlwp, cnp->cn_cred);
	if (!error) {
		nfsm_mtofh(dvp, newvp, v3, gotvp);
		if (!gotvp) {
			error = nfs_lookitup(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, cnp->cn_cred, curlwp, &np);
			if (!error)
				newvp = NFSTOV(np);
		}
	}
#ifndef NFS_V2_ONLY
	if (v3)
		nfsm_wcc_data(dvp, wccflag, 0, !error);
#endif
	nfsm_reqdone;
	if (error) {
		/*
		 * nfs_request maps NFSERR_NOTSUPP to ENOTSUP.
		 */
		if (v3 && error == ENOTSUP) {
			if (excl_mode == NFSV3CREATE_EXCLUSIVE) {
				excl_mode = NFSV3CREATE_GUARDED;
				goto again;
			} else if (excl_mode == NFSV3CREATE_GUARDED) {
				excl_mode = NFSV3CREATE_UNCHECKED;
				goto again;
			}
		}
	} else if (v3 && (excl_mode == NFSV3CREATE_EXCLUSIVE)) {
		struct timespec ts;

		getnanotime(&ts);

		/*
		 * make sure that we'll update timestamps as
		 * most server implementations use them to store
		 * the create verifier.
		 *
		 * XXX it's better to use TOSERVER always.
		 */

		if (vap->va_atime.tv_sec == VNOVAL)
			vap->va_atime = ts;
		if (vap->va_mtime.tv_sec == VNOVAL)
			vap->va_mtime = ts;

		error = nfs_setattrrpc(newvp, vap, cnp->cn_cred, curlwp);
	}
	if (error == 0) {
		if (cnp->cn_flags & MAKEENTRY)
			nfs_cache_enter(dvp, newvp, cnp);
		else
			cache_purge1(dvp, cnp->cn_nameptr, cnp->cn_namelen, 0);
		*ap->a_vpp = newvp;
		VOP_UNLOCK(newvp);
	} else {
		if (newvp)
			vput(newvp);
		if (error == EEXIST)
			cache_purge1(dvp, cnp->cn_nameptr, cnp->cn_namelen, 0);
	}
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
int
nfs_remove(void *v)
{
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nfsnode *np = VTONFS(vp);
	int error = 0;
	struct vattr vattr;

#ifndef DIAGNOSTIC
	if (vp->v_usecount < 1)
		panic("nfs_remove: bad v_usecount");
#endif
	if (vp->v_type == VDIR)
		error = EPERM;
	else if (vp->v_usecount == 1 || (np->n_sillyrename &&
	    VOP_GETATTR(vp, &vattr, cnp->cn_cred) == 0 &&
	    vattr.va_nlink > 1)) {
		/*
		 * Purge the name cache so that the chance of a lookup for
		 * the name succeeding while the remove is in progress is
		 * minimized. Without node locking it can still happen, such
		 * that an I/O op returns ESTALE, but since you get this if
		 * another host removes the file..
		 */
		cache_purge(vp);
		/*
		 * throw away biocache buffers, mainly to avoid
		 * unnecessary delayed writes later.
		 */
		error = nfs_vinvalbuf(vp, 0, cnp->cn_cred, curlwp, 1);
		/* Do the rpc */
		if (error != EINTR)
			error = nfs_removerpc(dvp, cnp->cn_nameptr,
				cnp->cn_namelen, cnp->cn_cred, curlwp);
	} else if (!np->n_sillyrename)
		error = nfs_sillyrename(dvp, vp, cnp, false);
	if (!error && nfs_getattrcache(vp, &vattr) == 0 &&
	    vattr.va_nlink == 1) {
		np->n_flag |= NREMOVED;
	}
	NFS_INVALIDATE_ATTRCACHE(np);
	VN_KNOTE(vp, NOTE_DELETE);
	VN_KNOTE(dvp, NOTE_WRITE);
	if (dvp == vp)
		vrele(vp);
	else
		vput(vp);
	vput(dvp);
	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
int
nfs_removeit(struct sillyrename *sp)
{

	return (nfs_removerpc(sp->s_dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		(struct lwp *)0));
}

/*
 * Nfs remove rpc, called from nfs_remove() and nfs_removeit().
 */
int
nfs_removerpc(struct vnode *dvp, const char *name, int namelen, kauth_cred_t cred, struct lwp *l)
{
	u_int32_t *tl;
	char *cp;
#ifndef NFS_V2_ONLY
	int32_t t1;
	char *cp2;
#endif
	int32_t t2;
	char *bpos, *dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(dvp);
	int rexmit = 0;
	struct nfsnode *dnp = VTONFS(dvp);

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	nfsm_reqhead(dnp, NFSPROC_REMOVE,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(namelen));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(name, namelen, NFS_MAXNAMLEN);
	nfsm_request1(dnp, NFSPROC_REMOVE, l, cred, &rexmit);
#ifndef NFS_V2_ONLY
	if (v3)
		nfsm_wcc_data(dvp, wccflag, 0, !error);
#endif
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	/*
	 * Kludge City: If the first reply to the remove rpc is lost..
	 *   the reply to the retransmitted request will be ENOENT
	 *   since the file was in fact removed
	 *   Therefore, we cheat and return success.
	 */
	if (rexmit && error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename call
 */
int
nfs_rename(void *v)
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	int error;

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	/*
	 * If the tvp exists and is in use, sillyrename it before doing the
	 * rename of the new file over it.
	 *
	 * Have sillyrename use link instead of rename if possible,
	 * so that we don't lose the file if the rename fails, and so
	 * that there's no window when the "to" file doesn't exist.
	 */
	if (tvp && tvp->v_usecount > 1 && !VTONFS(tvp)->n_sillyrename &&
	    tvp->v_type != VDIR && !nfs_sillyrename(tdvp, tvp, tcnp, true)) {
		VN_KNOTE(tvp, NOTE_DELETE);
		vput(tvp);
		tvp = NULL;
	}

	error = nfs_renamerpc(fdvp, fcnp->cn_nameptr, fcnp->cn_namelen,
		tdvp, tcnp->cn_nameptr, tcnp->cn_namelen, tcnp->cn_cred,
		curlwp);

	VN_KNOTE(fdvp, NOTE_WRITE);
	VN_KNOTE(tdvp, NOTE_WRITE);
	if (error == 0 || error == EEXIST) {
		if (fvp->v_type == VDIR)
			cache_purge(fvp);
		else
			cache_purge1(fdvp, fcnp->cn_nameptr, fcnp->cn_namelen,
				     0);
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tvp);
		else
			cache_purge1(tdvp, tcnp->cn_nameptr, tcnp->cn_namelen,
				     0);
	}
out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	return (error);
}

/*
 * nfs file rename rpc called from nfs_remove() above
 */
int
nfs_renameit(struct vnode *sdvp, struct componentname *scnp, struct sillyrename *sp)
{
	return (nfs_renamerpc(sdvp, scnp->cn_nameptr, scnp->cn_namelen,
		sdvp, sp->s_name, sp->s_namlen, scnp->cn_cred, curlwp));
}

/*
 * Do an nfs rename rpc. Called from nfs_rename() and nfs_renameit().
 */
int
nfs_renamerpc(struct vnode *fdvp, const char *fnameptr, int fnamelen, struct vnode *tdvp, const char *tnameptr, int tnamelen, kauth_cred_t cred, struct lwp *l)
{
	u_int32_t *tl;
	char *cp;
#ifndef NFS_V2_ONLY
	int32_t t1;
	char *cp2;
#endif
	int32_t t2;
	char *bpos, *dpos;
	int error = 0, fwccflag = NFSV3_WCCRATTR, twccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(fdvp);
	int rexmit = 0;
	struct nfsnode *fdnp = VTONFS(fdvp);

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(fdnp, NFSPROC_RENAME,
		(NFSX_FH(v3) + NFSX_UNSIGNED)*2 + nfsm_rndup(fnamelen) +
		nfsm_rndup(tnamelen));
	nfsm_fhtom(fdnp, v3);
	nfsm_strtom(fnameptr, fnamelen, NFS_MAXNAMLEN);
	nfsm_fhtom(VTONFS(tdvp), v3);
	nfsm_strtom(tnameptr, tnamelen, NFS_MAXNAMLEN);
	nfsm_request1(fdnp, NFSPROC_RENAME, l, cred, &rexmit);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_wcc_data(fdvp, fwccflag, 0, !error);
		nfsm_wcc_data(tdvp, twccflag, 0, !error);
	}
#endif
	nfsm_reqdone;
	VTONFS(fdvp)->n_flag |= NMODIFIED;
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	if (!fwccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(fdvp));
	if (!twccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(tdvp));
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (rexmit && error == ENOENT)
		error = 0;
	return (error);
}

/*
 * NFS link RPC, called from nfs_link.
 * Assumes dvp and vp locked, and leaves them that way.
 */

static int
nfs_linkrpc(struct vnode *dvp, struct vnode *vp, const char *name,
    size_t namelen, kauth_cred_t cred, struct lwp *l)
{
	u_int32_t *tl;
	char *cp;
#ifndef NFS_V2_ONLY
	int32_t t1;
	char *cp2;
#endif
	int32_t t2;
	char *bpos, *dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR, attrflag = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(dvp);
	int rexmit = 0;
	struct nfsnode *np = VTONFS(vp);

	nfsstats.rpccnt[NFSPROC_LINK]++;
	nfsm_reqhead(np, NFSPROC_LINK,
	    NFSX_FH(v3)*2 + NFSX_UNSIGNED + nfsm_rndup(namelen));
	nfsm_fhtom(np, v3);
	nfsm_fhtom(VTONFS(dvp), v3);
	nfsm_strtom(name, namelen, NFS_MAXNAMLEN);
	nfsm_request1(np, NFSPROC_LINK, l, cred, &rexmit);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_postop_attr(vp, attrflag, 0);
		nfsm_wcc_data(dvp, wccflag, 0, !error);
	}
#endif
	nfsm_reqdone;

	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!attrflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(vp));
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));

	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (rexmit && error == EEXIST)
		error = 0;

	return error;
}

/*
 * nfs hard link create call
 */
int
nfs_link(void *v)
{
	struct vop_link_v2_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	int error = 0;

	error = vn_lock(vp, LK_EXCLUSIVE);
	if (error != 0) {
		VOP_ABORTOP(dvp, cnp);
		return error;
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(vp, cnp->cn_cred, FSYNC_WAIT, 0, 0);

	error = nfs_linkrpc(dvp, vp, cnp->cn_nameptr, cnp->cn_namelen,
	    cnp->cn_cred, curlwp);

	if (error == 0) {
		cache_purge1(dvp, cnp->cn_nameptr, cnp->cn_namelen, 0);
	}
	VOP_UNLOCK(vp);
	VN_KNOTE(vp, NOTE_LINK);
	VN_KNOTE(dvp, NOTE_WRITE);
	return (error);
}

/*
 * nfs symbolic link create call
 */
int
nfs_symlink(void *v)
{
	struct vop_symlink_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	int slen, error = 0, wccflag = NFSV3_WCCRATTR, gotvp;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct vnode *newvp = (struct vnode *)0;
	const int v3 = NFS_ISV3(dvp);
	int rexmit = 0;
	struct nfsnode *dnp = VTONFS(dvp);

	*ap->a_vpp = NULL;
	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	slen = strlen(ap->a_target);
	nfsm_reqhead(dnp, NFSPROC_SYMLINK, NFSX_FH(v3) + 2*NFSX_UNSIGNED +
	    nfsm_rndup(cnp->cn_namelen) + nfsm_rndup(slen) + NFSX_SATTR(v3));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
#ifndef NFS_V2_ONlY
	if (v3)
		nfsm_v3attrbuild(vap, false);
#endif
	nfsm_strtom(ap->a_target, slen, NFS_MAXPATHLEN);
#ifndef NFS_V2_ONlY
	if (!v3) {
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VLNK, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
#endif
	nfsm_request1(dnp, NFSPROC_SYMLINK, curlwp, cnp->cn_cred,
	    &rexmit);
#ifndef NFS_V2_ONlY
	if (v3) {
		if (!error)
			nfsm_mtofh(dvp, newvp, v3, gotvp);
		nfsm_wcc_data(dvp, wccflag, 0, !error);
	}
#endif
	nfsm_reqdone;
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (rexmit && error == EEXIST)
		error = 0;
	if (error == 0 || error == EEXIST)
		cache_purge1(dvp, cnp->cn_nameptr, cnp->cn_namelen, 0);
	if (error == 0 && newvp == NULL) {
		struct nfsnode *np = NULL;

		error = nfs_lookitup(dvp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_cred, curlwp, &np);
		if (error == 0)
			newvp = NFSTOV(np);
	}
	if (error) {
		if (newvp != NULL)
			vput(newvp);
	} else {
		*ap->a_vpp = newvp;
		VOP_UNLOCK(newvp);
	}
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	VN_KNOTE(dvp, NOTE_WRITE);
	return (error);
}

/*
 * nfs make dir call
 */
int
nfs_mkdir(void *v)
{
	struct vop_mkdir_v3_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap = v;
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	int len;
	struct nfsnode *dnp = VTONFS(dvp), *np = (struct nfsnode *)0;
	struct vnode *newvp = (struct vnode *)0;
	char *bpos, *dpos, *cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int gotvp = 0;
	int rexmit = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(dvp);

	len = cnp->cn_namelen;
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	nfsm_reqhead(dnp, NFSPROC_MKDIR,
	  NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len) + NFSX_SATTR(v3));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
#ifndef NFS_V2_ONLY
	if (v3) {
		nfsm_v3attrbuild(vap, false);
	} else
#endif
	{
		nfsm_build(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		sp->sa_mode = vtonfsv2_mode(VDIR, vap->va_mode);
		sp->sa_uid = nfs_xdrneg1;
		sp->sa_gid = nfs_xdrneg1;
		sp->sa_size = nfs_xdrneg1;
		txdr_nfsv2time(&vap->va_atime, &sp->sa_atime);
		txdr_nfsv2time(&vap->va_mtime, &sp->sa_mtime);
	}
	nfsm_request1(dnp, NFSPROC_MKDIR, curlwp, cnp->cn_cred, &rexmit);
	if (!error)
		nfsm_mtofh(dvp, newvp, v3, gotvp);
	if (v3)
		nfsm_wcc_data(dvp, wccflag, 0, !error);
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	/*
	 * Kludge: Map EEXIST => 0 assuming that you have a reply to a retry
	 * if we can succeed in looking up the directory.
	 */
	if ((rexmit && error == EEXIST) || (!error && !gotvp)) {
		if (newvp) {
			vput(newvp);
			newvp = (struct vnode *)0;
		}
		error = nfs_lookitup(dvp, cnp->cn_nameptr, len, cnp->cn_cred,
			curlwp, &np);
		if (!error) {
			newvp = NFSTOV(np);
			if (newvp->v_type != VDIR || newvp == dvp)
				error = EEXIST;
		}
	}
	if (error) {
		if (newvp) {
			if (dvp != newvp)
				vput(newvp);
			else
				vrele(newvp);
		}
	} else {
		VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
		nfs_cache_enter(dvp, newvp, cnp);
		*ap->a_vpp = newvp;
		VOP_UNLOCK(newvp);
	}
	return (error);
}

/*
 * nfs remove directory call
 */
int
nfs_rmdir(void *v)
{
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	u_int32_t *tl;
	char *cp;
#ifndef NFS_V2_ONLY
	int32_t t1;
	char *cp2;
#endif
	int32_t t2;
	char *bpos, *dpos;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	int rexmit = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	const int v3 = NFS_ISV3(dvp);
	struct nfsnode *dnp;

	if (dvp == vp) {
		vrele(dvp);
		vput(dvp);
		return (EINVAL);
	}
	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	dnp = VTONFS(dvp);
	nfsm_reqhead(dnp, NFSPROC_RMDIR,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request1(dnp, NFSPROC_RMDIR, curlwp, cnp->cn_cred, &rexmit);
#ifndef NFS_V2_ONLY
	if (v3)
		nfsm_wcc_data(dvp, wccflag, 0, !error);
#endif
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	if (!wccflag)
		NFS_INVALIDATE_ATTRCACHE(VTONFS(dvp));
	VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	VN_KNOTE(vp, NOTE_DELETE);
	cache_purge(vp);
	vput(vp);
	vput(dvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (rexmit && error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs readdir call
 */
int
nfs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int *a_eofflag;
		off_t **a_cookies;
		int *a_ncookies;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	char *base = uio->uio_iov->iov_base;
	int tresid, error;
	size_t count, lost;
	struct dirent *dp;
	off_t *cookies = NULL;
	int ncookies = 0, nc;

	if (vp->v_type != VDIR)
		return (EPERM);

	lost = uio->uio_resid & (NFS_DIRFRAGSIZ - 1);
	count = uio->uio_resid - lost;
	if (count <= 0)
		return (EINVAL);

	/*
	 * Call nfs_bioread() to do the real work.
	 */
	tresid = uio->uio_resid = count;
	error = nfs_bioread(vp, uio, 0, ap->a_cred,
		    ap->a_cookies ? NFSBIO_CACHECOOKIES : 0);

	if (!error && ap->a_cookies) {
		ncookies = count / 16;
		cookies = malloc(sizeof (off_t) * ncookies, M_TEMP, M_WAITOK);
		*ap->a_cookies = cookies;
	}

	if (!error && uio->uio_resid == tresid) {
		uio->uio_resid += lost;
		nfsstats.direofcache_misses++;
		if (ap->a_cookies)
			*ap->a_ncookies = 0;
		*ap->a_eofflag = 1;
		return (0);
	}

	if (!error && ap->a_cookies) {
		/*
		 * Only the NFS server and emulations use cookies, and they
		 * load the directory block into system space, so we can
		 * just look at it directly.
		 */
		if (!VMSPACE_IS_KERNEL_P(uio->uio_vmspace) ||
		    uio->uio_iovcnt != 1)
			panic("nfs_readdir: lost in space");
		for (nc = 0; ncookies-- &&
		     base < (char *)uio->uio_iov->iov_base; nc++){
			dp = (struct dirent *) base;
			if (dp->d_reclen == 0)
				break;
			if (nmp->nm_flag & NFSMNT_XLATECOOKIE)
				*(cookies++) = (off_t)NFS_GETCOOKIE32(dp);
			else
				*(cookies++) = NFS_GETCOOKIE(dp);
			base += dp->d_reclen;
		}
		uio->uio_resid +=
		    ((char *)uio->uio_iov->iov_base - base);
		uio->uio_iov->iov_len +=
		    ((char *)uio->uio_iov->iov_base - base);
		uio->uio_iov->iov_base = base;
		*ap->a_ncookies = nc;
	}

	uio->uio_resid += lost;
	*ap->a_eofflag = 0;
	return (error);
}

/*
 * Readdir rpc call.
 * Called from below the buffer cache by nfs_doio().
 */
int
nfs_readdirrpc(struct vnode *vp, struct uio *uiop, kauth_cred_t cred)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp);
	u_quad_t fileno;
	int error = 0, more_dirs = 1, blksiz = 0, bigenough = 1;
#ifndef NFS_V2_ONLY
	int attrflag;
#endif
	int nrpcs = 0, reclen;
	const int v3 = NFS_ISV3(vp);

#ifdef DIAGNOSTIC
	/*
	 * Should be called from buffer cache, so only amount of
	 * NFS_DIRBLKSIZ will be requested.
	 */
	if (uiop->uio_iovcnt != 1 || uiop->uio_resid != NFS_DIRBLKSIZ)
		panic("nfs readdirrpc bad uio");
#endif

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of NFS_DIRFRAGSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		/*
		 * Heuristic: don't bother to do another RPC to further
		 * fill up this block if there is not much room left. (< 50%
		 * of the readdir RPC size). This wastes some buffer space
		 * but can save up to 50% in RPC calls.
		 */
		if (nrpcs > 0 && uiop->uio_resid < (nmp->nm_readdirsize / 2)) {
			bigenough = 0;
			break;
		}
		nfsstats.rpccnt[NFSPROC_READDIR]++;
		nfsm_reqhead(dnp, NFSPROC_READDIR, NFSX_FH(v3) +
			NFSX_READDIR(v3));
		nfsm_fhtom(dnp, v3);
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_build(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE) {
				txdr_swapcookie3(uiop->uio_offset, tl);
			} else {
				txdr_cookie3(uiop->uio_offset, tl);
			}
			tl += 2;
			*tl++ = dnp->n_cookieverf.nfsuquad[0];
			*tl++ = dnp->n_cookieverf.nfsuquad[1];
		} else
#endif
		{
			nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(uiop->uio_offset);
		}
		*tl = txdr_unsigned(nmp->nm_readdirsize);
		nfsm_request(dnp, NFSPROC_READDIR, curlwp, cred);
		nrpcs++;
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(vp, attrflag, 0);
			if (!error) {
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
				dnp->n_cookieverf.nfsuquad[0] = *tl++;
				dnp->n_cookieverf.nfsuquad[1] = *tl;
			} else {
				m_freem(mrep);
				goto nfsmout;
			}
		}
#endif
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);

		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
#ifndef NFS_V2_ONLY
			if (v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
				fileno = fxdr_hyper(tl);
				len = fxdr_unsigned(int, *(tl + 2));
			} else
#endif
			{
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
				fileno = fxdr_unsigned(u_quad_t, *tl++);
				len = fxdr_unsigned(int, *tl);
			}
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			/* for cookie stashing */
			reclen = _DIRENT_RECLEN(dp, len) + 2 * sizeof(off_t);
			left = NFS_DIRFRAGSIZ - blksiz;
			if (reclen > left) {
				memset(uiop->uio_iov->iov_base, 0, left);
				dp->d_reclen += left;
				UIO_ADVANCE(uiop, left);
				blksiz = 0;
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
			}
			if (reclen > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				int tlen;

				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = fileno;
				dp->d_namlen = len;
				dp->d_reclen = reclen;
				dp->d_type = DT_UNKNOWN;
				blksiz += reclen;
				if (blksiz == NFS_DIRFRAGSIZ)
					blksiz = 0;
				UIO_ADVANCE(uiop, DIRHDSIZ);
				nfsm_mtouio(uiop, len);
				tlen = reclen - (DIRHDSIZ + len);
				(void)memset(uiop->uio_iov->iov_base, 0, tlen);
				UIO_ADVANCE(uiop, tlen);
			} else
				nfsm_adv(nfsm_rndup(len));
#ifndef NFS_V2_ONLY
			if (v3) {
				nfsm_dissect(tl, u_int32_t *,
				    3 * NFSX_UNSIGNED);
			} else
#endif
			{
				nfsm_dissect(tl, u_int32_t *,
				    2 * NFSX_UNSIGNED);
			}
			if (bigenough) {
#ifndef NFS_V2_ONLY
				if (v3) {
					if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE)
						uiop->uio_offset =
						    fxdr_swapcookie3(tl);
					else
						uiop->uio_offset =
						    fxdr_cookie3(tl);
				}
				else
#endif
				{
					uiop->uio_offset =
					    fxdr_unsigned(off_t, *tl);
				}
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
			}
			if (v3)
				tl += 2;
			else
				tl++;
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);

			/*
			 * kludge: if we got no entries, treat it as EOF.
			 * some server sometimes send a reply without any
			 * entries or EOF.
			 * although it might mean the server has very long name,
			 * we can't handle such entries anyway.
			 */

			if (uiop->uio_resid >= NFS_DIRBLKSIZ)
				more_dirs = 0;
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_DIRFRAGSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_DIRFRAGSIZ - blksiz;
		memset(uiop->uio_iov->iov_base, 0, left);
		dp->d_reclen += left;
		NFS_STASHCOOKIE(dp, uiop->uio_offset);
		UIO_ADVANCE(uiop, left);
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough) {
		dnp->n_direofoffset = uiop->uio_offset;
		dnp->n_flag |= NEOFVALID;
	}
nfsmout:
	return (error);
}

#ifndef NFS_V2_ONLY
/*
 * NFS V3 readdir plus RPC. Used in place of nfs_readdirrpc().
 */
int
nfs_readdirplusrpc(struct vnode *vp, struct uio *uiop, kauth_cred_t cred)
{
	int len, left;
	struct dirent *dp = NULL;
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	struct vnode *newvp;
	char *bpos, *dpos, *cp2;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsnode *dnp = VTONFS(vp), *np;
	nfsfh_t *fhp;
	u_quad_t fileno;
	int error = 0, more_dirs = 1, blksiz = 0, doit, bigenough = 1, i;
	int attrflag, fhsize, nrpcs = 0, reclen;
	struct nfs_fattr fattr, *fp;

#ifdef DIAGNOSTIC
	if (uiop->uio_iovcnt != 1 || uiop->uio_resid != NFS_DIRBLKSIZ)
		panic("nfs readdirplusrpc bad uio");
#endif
	ndp->ni_dvp = vp;
	newvp = NULLVP;

	/*
	 * Loop around doing readdir rpc's of size nm_readdirsize
	 * truncated to a multiple of NFS_DIRFRAGSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && bigenough) {
		if (nrpcs > 0 && uiop->uio_resid < (nmp->nm_readdirsize / 2)) {
			bigenough = 0;
			break;
		}
		nfsstats.rpccnt[NFSPROC_READDIRPLUS]++;
		nfsm_reqhead(dnp, NFSPROC_READDIRPLUS,
			NFSX_FH(1) + 6 * NFSX_UNSIGNED);
		nfsm_fhtom(dnp, 1);
 		nfsm_build(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
		if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE) {
			txdr_swapcookie3(uiop->uio_offset, tl);
		} else {
			txdr_cookie3(uiop->uio_offset, tl);
		}
		tl += 2;
		*tl++ = dnp->n_cookieverf.nfsuquad[0];
		*tl++ = dnp->n_cookieverf.nfsuquad[1];
		*tl++ = txdr_unsigned(nmp->nm_readdirsize);
		*tl = txdr_unsigned(nmp->nm_rsize);
		nfsm_request(dnp, NFSPROC_READDIRPLUS, curlwp, cred);
		nfsm_postop_attr(vp, attrflag, 0);
		if (error) {
			m_freem(mrep);
			goto nfsmout;
		}
		nrpcs++;
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		dnp->n_cookieverf.nfsuquad[0] = *tl++;
		dnp->n_cookieverf.nfsuquad[1] = *tl++;
		more_dirs = fxdr_unsigned(int, *tl);

		/* loop thru the dir entries, doctoring them to 4bsd form */
		while (more_dirs && bigenough) {
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			fileno = fxdr_hyper(tl);
			len = fxdr_unsigned(int, *(tl + 2));
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			/* for cookie stashing */
			reclen = _DIRENT_RECLEN(dp, len) + 2 * sizeof(off_t);
			left = NFS_DIRFRAGSIZ - blksiz;
			if (reclen > left) {
				/*
				 * DIRFRAGSIZ is aligned, no need to align
				 * again here.
				 */
				memset(uiop->uio_iov->iov_base, 0, left);
				dp->d_reclen += left;
				UIO_ADVANCE(uiop, left);
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
				blksiz = 0;
			}
			if (reclen > uiop->uio_resid)
				bigenough = 0;
			if (bigenough) {
				int tlen;

				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = fileno;
				dp->d_namlen = len;
				dp->d_reclen = reclen;
				dp->d_type = DT_UNKNOWN;
				blksiz += reclen;
				if (blksiz == NFS_DIRFRAGSIZ)
					blksiz = 0;
				UIO_ADVANCE(uiop, DIRHDSIZ);
				nfsm_mtouio(uiop, len);
				tlen = reclen - (DIRHDSIZ + len);
				(void)memset(uiop->uio_iov->iov_base, 0, tlen);
				UIO_ADVANCE(uiop, tlen);
				cnp->cn_nameptr = dp->d_name;
				cnp->cn_namelen = dp->d_namlen;
			} else
				nfsm_adv(nfsm_rndup(len));
			nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			if (bigenough) {
				if (nmp->nm_iflag & NFSMNT_SWAPCOOKIE)
					uiop->uio_offset =
						fxdr_swapcookie3(tl);
				else
					uiop->uio_offset =
						fxdr_cookie3(tl);
				NFS_STASHCOOKIE(dp, uiop->uio_offset);
			}
			tl += 2;

			/*
			 * Since the attributes are before the file handle
			 * (sigh), we must skip over the attributes and then
			 * come back and get them.
			 */
			attrflag = fxdr_unsigned(int, *tl);
			if (attrflag) {
			    nfsm_dissect(fp, struct nfs_fattr *, NFSX_V3FATTR);
			    memcpy(&fattr, fp, NFSX_V3FATTR);
			    nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			    doit = fxdr_unsigned(int, *tl);
			    if (doit) {
				nfsm_getfh(fhp, fhsize, 1);
				if (NFS_CMPFH(dnp, fhp, fhsize)) {
				    vref(vp);
				    newvp = vp;
				    np = dnp;
				} else {
				    error = nfs_nget1(vp->v_mount, fhp,
					fhsize, &np, LK_NOWAIT);
				    if (!error)
					newvp = NFSTOV(np);
				}
				if (!error) {
				    nfs_loadattrcache(&newvp, &fattr, 0, 0);
				    if (bigenough) {
					dp->d_type =
					   IFTODT(VTTOIF(np->n_vattr->va_type));
					if (cnp->cn_namelen <= NCHNAMLEN) {
					    ndp->ni_vp = newvp;
					    nfs_cache_enter(ndp->ni_dvp,
						ndp->ni_vp, cnp);
					}
				    }
				}
				error = 0;
			   }
			} else {
			    /* Just skip over the file handle */
			    nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			    i = fxdr_unsigned(int, *tl);
			    nfsm_adv(nfsm_rndup(i));
			}
			if (newvp != NULLVP) {
			    if (newvp == vp)
				vrele(newvp);
			    else
				vput(newvp);
			    newvp = NULLVP;
			}
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);

			/*
			 * kludge: see a comment in nfs_readdirrpc.
			 */

			if (uiop->uio_resid >= NFS_DIRBLKSIZ)
				more_dirs = 0;
		}
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_DIRFRAGSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (blksiz > 0) {
		left = NFS_DIRFRAGSIZ - blksiz;
		memset(uiop->uio_iov->iov_base, 0, left);
		dp->d_reclen += left;
		NFS_STASHCOOKIE(dp, uiop->uio_offset);
		UIO_ADVANCE(uiop, left);
	}

	/*
	 * We are now either at the end of the directory or have filled the
	 * block.
	 */
	if (bigenough) {
		dnp->n_direofoffset = uiop->uio_offset;
		dnp->n_flag |= NEOFVALID;
	}
nfsmout:
	if (newvp != NULLVP) {
		if(newvp == vp)
		    vrele(newvp);
		else
		    vput(newvp);
	}
	return (error);
}
#endif

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
int
nfs_sillyrename(struct vnode *dvp, struct vnode *vp, struct componentname *cnp, bool dolink)
{
	struct sillyrename *sp;
	struct nfsnode *np;
	int error;
	pid_t pid;

	cache_purge(dvp);
	np = VTONFS(vp);
#ifndef DIAGNOSTIC
	if (vp->v_type == VDIR)
		panic("nfs: sillyrename dir");
#endif
	sp = kmem_alloc(sizeof(*sp), KM_SLEEP);
	sp->s_cred = kauth_cred_dup(cnp->cn_cred);
	sp->s_dvp = dvp;
	vref(dvp);

	/* Fudge together a funny name */
	pid = curlwp->l_proc->p_pid;
	memcpy(sp->s_name, ".nfsAxxxx4.4", 13);
	sp->s_namlen = 12;
	sp->s_name[8] = hexdigits[pid & 0xf];
	sp->s_name[7] = hexdigits[(pid >> 4) & 0xf];
	sp->s_name[6] = hexdigits[(pid >> 8) & 0xf];
	sp->s_name[5] = hexdigits[(pid >> 12) & 0xf];

	/* Try lookitups until we get one that isn't there */
	while (nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		curlwp, (struct nfsnode **)0) == 0) {
		sp->s_name[4]++;
		if (sp->s_name[4] > 'z') {
			error = EINVAL;
			goto bad;
		}
	}
	if (dolink) {
		error = nfs_linkrpc(dvp, vp, sp->s_name, sp->s_namlen,
		    sp->s_cred, curlwp);
		/*
		 * nfs_request maps NFSERR_NOTSUPP to ENOTSUP.
		 */
		if (error == ENOTSUP) {
			error = nfs_renameit(dvp, cnp, sp);
		}
	} else {
		error = nfs_renameit(dvp, cnp, sp);
	}
	if (error)
		goto bad;
	error = nfs_lookitup(dvp, sp->s_name, sp->s_namlen, sp->s_cred,
		curlwp, &np);
	np->n_sillyrename = sp;
	return (0);
bad:
	vrele(sp->s_dvp);
	kauth_cred_free(sp->s_cred);
	kmem_free(sp, sizeof(*sp));
	return (error);
}

/*
 * Look up a file name and optionally either update the file handle or
 * allocate an nfsnode, depending on the value of npp.
 * npp == NULL	--> just do the lookup
 * *npp == NULL --> allocate a new nfsnode and make sure attributes are
 *			handled too
 * *npp != NULL --> update the file handle in the vnode
 */
int
nfs_lookitup(struct vnode *dvp, const char *name, int len, kauth_cred_t cred, struct lwp *l, struct nfsnode **npp)
{
	u_int32_t *tl;
	char *cp;
	int32_t t1, t2;
	struct vnode *newvp = (struct vnode *)0;
	struct nfsnode *np, *dnp = VTONFS(dvp);
	char *bpos, *dpos, *cp2;
	int error = 0, ofhlen, fhlen;
#ifndef NFS_V2_ONLY
	int attrflag;
#endif
	struct mbuf *mreq, *mrep, *md, *mb;
	nfsfh_t *ofhp, *nfhp;
	const int v3 = NFS_ISV3(dvp);

	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	nfsm_reqhead(dnp, NFSPROC_LOOKUP,
		NFSX_FH(v3) + NFSX_UNSIGNED + nfsm_rndup(len));
	nfsm_fhtom(dnp, v3);
	nfsm_strtom(name, len, NFS_MAXNAMLEN);
	nfsm_request(dnp, NFSPROC_LOOKUP, l, cred);
	if (npp && !error) {
		nfsm_getfh(nfhp, fhlen, v3);
		if (*npp) {
		    np = *npp;
		    newvp = NFSTOV(np);
		    ofhlen = np->n_fhsize;
		    ofhp = kmem_alloc(ofhlen, KM_SLEEP);
		    memcpy(ofhp, np->n_fhp, ofhlen);
		    error = vcache_rekey_enter(newvp->v_mount, newvp,
			ofhp, ofhlen, nfhp, fhlen);
		    if (error) {
			kmem_free(ofhp, ofhlen);
			m_freem(mrep);
			return error;
		    }
		    if (np->n_fhsize > NFS_SMALLFH && fhlen <= NFS_SMALLFH) {
			kmem_free(np->n_fhp, np->n_fhsize);
			np->n_fhp = &np->n_fh;
		    }
#if NFS_SMALLFH < NFSX_V3FHMAX
		    else if (np->n_fhsize <= NFS_SMALLFH && fhlen > NFS_SMALLFH)
			np->n_fhp = kmem_alloc(fhlen, KM_SLEEP);
#endif
		    memcpy(np->n_fhp, nfhp, fhlen);
		    np->n_fhsize = fhlen;
		    vcache_rekey_exit(newvp->v_mount, newvp,
			ofhp, ofhlen, np->n_fhp, fhlen);
		    kmem_free(ofhp, ofhlen);
		} else if (NFS_CMPFH(dnp, nfhp, fhlen)) {
		    vref(dvp);
		    newvp = dvp;
		    np = dnp;
		} else {
		    error = nfs_nget(dvp->v_mount, nfhp, fhlen, &np);
		    if (error) {
			m_freem(mrep);
			return (error);
		    }
		    newvp = NFSTOV(np);
		}
#ifndef NFS_V2_ONLY
		if (v3) {
			nfsm_postop_attr(newvp, attrflag, 0);
			if (!attrflag && *npp == NULL) {
				m_freem(mrep);
				vput(newvp);
				return (ENOENT);
			}
		} else
#endif
			nfsm_loadattr(newvp, (struct vattr *)0, 0);
	}
	nfsm_reqdone;
	if (npp && *npp == NULL) {
		if (error) {
			if (newvp)
				vput(newvp);
		} else
			*npp = np;
	}
	return (error);
}

#ifndef NFS_V2_ONLY
/*
 * Nfs Version 3 commit rpc
 */
int
nfs_commit(struct vnode *vp, off_t offset, uint32_t cnt, struct lwp *l)
{
	char *cp;
	u_int32_t *tl;
	int32_t t1, t2;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	char *bpos, *dpos, *cp2;
	int error = 0, wccflag = NFSV3_WCCRATTR;
	struct mbuf *mreq, *mrep, *md, *mb;
	struct nfsnode *np;

	KASSERT(NFS_ISV3(vp));

#ifdef NFS_DEBUG_COMMIT
	printf("commit %lu - %lu\n", (unsigned long)offset,
	    (unsigned long)(offset + cnt));
#endif

	mutex_enter(&nmp->nm_lock);
	if ((nmp->nm_iflag & NFSMNT_HASWRITEVERF) == 0) {
		mutex_exit(&nmp->nm_lock);
		return (0);
	}
	mutex_exit(&nmp->nm_lock);
	nfsstats.rpccnt[NFSPROC_COMMIT]++;
	np = VTONFS(vp);
	nfsm_reqhead(np, NFSPROC_COMMIT, NFSX_FH(1));
	nfsm_fhtom(np, 1);
	nfsm_build(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	txdr_hyper(offset, tl);
	tl += 2;
	*tl = txdr_unsigned(cnt);
	nfsm_request(np, NFSPROC_COMMIT, l, np->n_wcred);
	nfsm_wcc_data(vp, wccflag, NAC_NOTRUNC, false);
	if (!error) {
		nfsm_dissect(tl, u_int32_t *, NFSX_V3WRITEVERF);
		mutex_enter(&nmp->nm_lock);
		if ((nmp->nm_iflag & NFSMNT_STALEWRITEVERF) ||
		    memcmp(nmp->nm_writeverf, tl, NFSX_V3WRITEVERF)) {
			memcpy(nmp->nm_writeverf, tl, NFSX_V3WRITEVERF);
			error = NFSERR_STALEWRITEVERF;
			nmp->nm_iflag |= NFSMNT_STALEWRITEVERF;
		}
		mutex_exit(&nmp->nm_lock);
	}
	nfsm_reqdone;
	return (error);
}
#endif

/*
 * Kludge City..
 * - make nfs_bmap() essentially a no-op that does no translation
 * - do nfs_strategy() by doing I/O with nfs_readrpc/nfs_writerpc
 *   (Maybe I could use the process's page mapping, but I was concerned that
 *    Kernel Write might not be enabled and also figured copyout() would do
 *    a lot more work than memcpy() and also it currently happens in the
 *    context of the swapper process (2).
 */
int
nfs_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int bshift = vp->v_mount->mnt_fs_bshift - vp->v_mount->mnt_dev_bshift;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn << bshift;
	if (ap->a_runp != NULL)
		*ap->a_runp = 1024 * 1024; /* XXX */
	return (0);
}

/*
 * Strategy routine.
 * For async requests when nfsiod(s) are running, queue the request by
 * calling nfs_asyncio(), otherwise just all nfs_doio() to do the
 * request.
 */
int
nfs_strategy(void *v)
{
	struct vop_strategy_args *ap = v;
	struct buf *bp = ap->a_bp;
	int error = 0;

	if ((bp->b_flags & (B_PHYS|B_ASYNC)) == (B_PHYS|B_ASYNC))
		panic("nfs physio/async");

	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 || nfs_asyncio(bp))
		error = nfs_doio(bp);
	return (error);
}

/*
 * fsync vnode op. Just call nfs_flush() with commit == 1.
 */
/* ARGSUSED */
int
nfs_fsync(void *v)
{
	struct vop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_vp;
		kauth_cred_t  a_cred;
		int  a_flags;
		off_t offlo;
		off_t offhi;
		struct lwp * a_l;
	} */ *ap = v;

	struct vnode *vp = ap->a_vp;

	if (vp->v_type != VREG)
		return 0;

	return (nfs_flush(vp, ap->a_cred,
	    (ap->a_flags & FSYNC_WAIT) != 0 ? MNT_WAIT : 0, curlwp, 1));
}

/*
 * Flush all the data associated with a vnode.
 */
int
nfs_flush(struct vnode *vp, kauth_cred_t cred, int waitfor, struct lwp *l,
    int commit)
{
	struct nfsnode *np = VTONFS(vp);
	int error;
	int flushflags = PGO_ALLPAGES|PGO_CLEANIT|PGO_SYNCIO;
	UVMHIST_FUNC("nfs_flush"); UVMHIST_CALLED(ubchist);

	mutex_enter(vp->v_interlock);
	error = VOP_PUTPAGES(vp, 0, 0, flushflags);
	if (np->n_flag & NWRITEERR) {
		error = np->n_error;
		np->n_flag &= ~NWRITEERR;
	}
	UVMHIST_LOG(ubchist, "returning %d", error,0,0,0);
	return (error);
}

/*
 * Return POSIX pathconf information applicable to nfs.
 *
 * N.B. The NFS V2 protocol doesn't support this RPC.
 */
/* ARGSUSED */
int
nfs_pathconf(void *v)
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;
	struct nfsv3_pathconf *pcp;
	struct vnode *vp = ap->a_vp;
	struct mbuf *mreq, *mrep, *md, *mb;
	int32_t t1, t2;
	u_int32_t *tl;
	char *bpos, *dpos, *cp, *cp2;
	int error = 0, attrflag;
#ifndef NFS_V2_ONLY
	struct nfsmount *nmp;
	unsigned int l;
	u_int64_t maxsize;
#endif
	const int v3 = NFS_ISV3(vp);
	struct nfsnode *np = VTONFS(vp);

	switch (ap->a_name) {
		/* Names that can be resolved locally. */
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 1;
		break;
	/* Names that cannot be resolved locally; do an RPC, if possible. */
	case _PC_LINK_MAX:
	case _PC_NAME_MAX:
	case _PC_CHOWN_RESTRICTED:
	case _PC_NO_TRUNC:
		if (!v3) {
			error = EINVAL;
			break;
		}
		nfsstats.rpccnt[NFSPROC_PATHCONF]++;
		nfsm_reqhead(np, NFSPROC_PATHCONF, NFSX_FH(1));
		nfsm_fhtom(np, 1);
		nfsm_request(np, NFSPROC_PATHCONF,
		    curlwp, curlwp->l_cred);	/* XXX */
		nfsm_postop_attr(vp, attrflag, 0);
		if (!error) {
			nfsm_dissect(pcp, struct nfsv3_pathconf *,
			    NFSX_V3PATHCONF);
			switch (ap->a_name) {
			case _PC_LINK_MAX:
				*ap->a_retval =
				    fxdr_unsigned(register_t, pcp->pc_linkmax);
				break;
			case _PC_NAME_MAX:
				*ap->a_retval =
				    fxdr_unsigned(register_t, pcp->pc_namemax);
				break;
			case _PC_CHOWN_RESTRICTED:
				*ap->a_retval =
				    (pcp->pc_chownrestricted == nfs_true);
				break;
			case _PC_NO_TRUNC:
				*ap->a_retval =
				    (pcp->pc_notrunc == nfs_true);
				break;
			}
		}
		nfsm_reqdone;
		break;
	case _PC_FILESIZEBITS:
#ifndef NFS_V2_ONLY
		if (v3) {
			nmp = VFSTONFS(vp->v_mount);
			if ((nmp->nm_iflag & NFSMNT_GOTFSINFO) == 0)
				if ((error = nfs_fsinfo(nmp, vp,
				    curlwp->l_cred, curlwp)) != 0) /* XXX */
					break;
			for (l = 0, maxsize = nmp->nm_maxfilesize;
			    (maxsize >> l) > 0; l++)
				;
			*ap->a_retval = l + 1;
		} else
#endif
		{
			*ap->a_retval = 32;	/* NFS V2 limitation */
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * NFS advisory byte-level locks.
 */
int
nfs_advlock(void *v)
{
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		void *a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	return lf_advlock(ap, &np->n_lockf, np->n_size);
}

/*
 * Print out the contents of an nfsnode.
 */
int
nfs_print(void *v)
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);

	printf("tag VT_NFS, fileid %lld fsid 0x%llx",
	    (unsigned long long)np->n_vattr->va_fileid, 
	    (unsigned long long)np->n_vattr->va_fsid);
	if (vp->v_type == VFIFO)
		VOCALL(fifo_vnodeop_p, VOFFSET(vop_print), v);
	printf("\n");
	return (0);
}

/*
 * nfs unlock wrapper.
 */
int
nfs_unlock(void *v)
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	/*
	 * VOP_UNLOCK can be called by nfs_loadattrcache
	 * with v_data == 0.
	 */
	if (VTONFS(vp)) {
		nfs_delayedtruncate(vp);
	}

	return genfs_unlock(v);
}

/*
 * nfs special file access vnode op.
 * Essentially just get vattr and then imitate iaccess() since the device is
 * local to the client.
 */
int
nfsspec_access(void *v)
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vattr va;
	struct vnode *vp = ap->a_vp;
	int error;

	error = VOP_GETATTR(vp, &va, ap->a_cred);
	if (error)
		return (error);

        /*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}

	return kauth_authorize_vnode(ap->a_cred, KAUTH_ACCESS_ACTION(ap->a_mode,
	    va.va_type, va.va_mode), vp, NULL, genfs_can_access(va.va_type,
	    va.va_mode, va.va_uid, va.va_gid, ap->a_mode, ap->a_cred));
}

/*
 * Read wrapper for special devices.
 */
int
nfsspec_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
int
nfsspec_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the nfsnode then do device close.
 */
int
nfsspec_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			vattr_null(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred);
		}
	}
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Read wrapper for fifos.
 */
int
nfsfifo_read(void *v)
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	getnanotime(&np->n_atim);
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifos.
 */
int
nfsfifo_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	getnanotime(&np->n_mtim);
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the nfsnode then do fifo close.
 */
int
nfsfifo_close(void *v)
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		kauth_cred_t a_cred;
		struct lwp *a_l;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		struct timespec ts;

		getnanotime(&ts);
		if (np->n_flag & NACC)
			np->n_atim = ts;
		if (np->n_flag & NUPD)
			np->n_mtim = ts;
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			vattr_null(&vattr);
			if (np->n_flag & NACC)
				vattr.va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vattr.va_mtime = np->n_mtim;
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred);
		}
	}
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_close), ap));
}
