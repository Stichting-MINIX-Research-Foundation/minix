/*	$NetBSD: nfs_vfsops.c,v 1.230 2015/07/15 03:28:55 manu Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1995
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
 *	@(#)nfs_vfsops.c	8.12 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_vfsops.c,v 1.230 2015/07/15 03:28:55 manu Exp $");

#if defined(_KERNEL_OPT)
#include "opt_nfs.h"
#endif

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/dirent.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsdiskless.h>
#include <nfs/nfs_var.h>

MODULE(MODULE_CLASS_VFS, nfs, NULL);

extern struct nfsstats nfsstats;
extern int nfs_ticks;

/*
 * keep a count of the nfs mounts to generate ficticious drive names
 * for the per drive stats.
 */
unsigned int nfs_mount_count = 0;

int nfs_commitsize;

/*
 * nfs vfs operations.
 */

extern const struct vnodeopv_desc nfsv2_vnodeop_opv_desc;
extern const struct vnodeopv_desc spec_nfsv2nodeop_opv_desc;
extern const struct vnodeopv_desc fifo_nfsv2nodeop_opv_desc;

const struct vnodeopv_desc * const nfs_vnodeopv_descs[] = {
	&nfsv2_vnodeop_opv_desc,
	&spec_nfsv2nodeop_opv_desc,
	&fifo_nfsv2nodeop_opv_desc,
	NULL,
};

struct vfsops nfs_vfsops = {
	.vfs_name = MOUNT_NFS,
	.vfs_min_mount_data = sizeof (struct nfs_args),
	.vfs_mount = nfs_mount,
	.vfs_start = nfs_start,
	.vfs_unmount = nfs_unmount,
	.vfs_root = nfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = nfs_statvfs,
	.vfs_sync = nfs_sync,
	.vfs_loadvnode = nfs_loadvnode,
	.vfs_vget = nfs_vget,
	.vfs_fhtovp = nfs_fhtovp,
	.vfs_vptofh = nfs_vptofh,
	.vfs_init = nfs_vfs_init,
	.vfs_done = nfs_vfs_done,
	.vfs_mountroot = nfs_mountroot,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = nfs_vnodeopv_descs
};

extern u_int32_t nfs_procids[NFS_NPROCS];
extern u_int32_t nfs_prog, nfs_vers;
static struct sysctllog *nfs_clog;

static int nfs_mount_diskless(struct nfs_dlmount *, const char *,
    struct mount **, struct vnode **, struct lwp *);
static void nfs_sysctl_init(void);
static void nfs_sysctl_fini(void);

static int
nfs_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = vfs_attach(&nfs_vfsops);
		if (error == 0) {
			nfs_sysctl_init();
		}
		return error;
	case MODULE_CMD_FINI:
		error = vfs_detach(&nfs_vfsops);
		if (error == 0) {
			nfs_sysctl_fini();
		}
		return error;
	default:
		return ENOTTY;
	}
}

/*
 * nfs statvfs call
 */
int
nfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	struct lwp *l = curlwp;
	struct vnode *vp;
	struct nfs_statfs *sfp;
	char *cp;
	u_int32_t *tl;
	int32_t t1, t2;
	char *bpos, *dpos, *cp2;
	struct nfsmount *nmp = VFSTONFS(mp);
	int error = 0, retattr;
#ifdef NFS_V2_ONLY
	const int v3 = 0;
#else
	int v3 = (nmp->nm_flag & NFSMNT_NFSV3);
#endif
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	kauth_cred_t cred;
	u_quad_t tquad;
	struct nfsnode *np;

#ifndef nolint
	sfp = (struct nfs_statfs *)0;
#endif
	vp = nmp->nm_vnode;
	np = VTONFS(vp);
	cred = kauth_cred_alloc();
#ifndef NFS_V2_ONLY
	if (v3 && (nmp->nm_iflag & NFSMNT_GOTFSINFO) == 0)
		(void)nfs_fsinfo(nmp, vp, cred, l);
#endif
	nfsstats.rpccnt[NFSPROC_FSSTAT]++;
	nfsm_reqhead(np, NFSPROC_FSSTAT, NFSX_FH(v3));
	nfsm_fhtom(np, v3);
	nfsm_request(np, NFSPROC_FSSTAT, l, cred);
	if (v3)
		nfsm_postop_attr(vp, retattr, 0);
	if (error) {
		if (mrep != NULL) {
			if (mrep->m_next != NULL)
				printf("nfs_vfsops: nfs_statvfs would lose buffers\n");
			m_freem(mrep);
		}
		goto nfsmout;
	}
	nfsm_dissect(sfp, struct nfs_statfs *, NFSX_STATFS(v3));
	sbp->f_flag = nmp->nm_flag;
	sbp->f_iosize = min(nmp->nm_rsize, nmp->nm_wsize);
	if (v3) {
		sbp->f_frsize = sbp->f_bsize = NFS_FABLKSIZE;
		tquad = fxdr_hyper(&sfp->sf_tbytes);
		sbp->f_blocks = ((quad_t)tquad / (quad_t)NFS_FABLKSIZE);
		tquad = fxdr_hyper(&sfp->sf_fbytes);
		sbp->f_bfree = ((quad_t)tquad / (quad_t)NFS_FABLKSIZE);
		tquad = fxdr_hyper(&sfp->sf_abytes);
		tquad = ((quad_t)tquad / (quad_t)NFS_FABLKSIZE);
		sbp->f_bresvd = sbp->f_bfree - tquad;
		sbp->f_bavail = tquad;
		/* Handle older NFS servers returning negative values */
		if ((quad_t)sbp->f_bavail < 0)
			sbp->f_bavail = 0;
		tquad = fxdr_hyper(&sfp->sf_tfiles);
		sbp->f_files = tquad;
		tquad = fxdr_hyper(&sfp->sf_ffiles);
		sbp->f_ffree = tquad;
		sbp->f_favail = tquad;
		sbp->f_fresvd = 0;
		sbp->f_namemax = NFS_MAXNAMLEN;
	} else {
		sbp->f_bsize = NFS_FABLKSIZE;
		sbp->f_frsize = fxdr_unsigned(int32_t, sfp->sf_bsize);
		sbp->f_blocks = fxdr_unsigned(int32_t, sfp->sf_blocks);
		sbp->f_bfree = fxdr_unsigned(int32_t, sfp->sf_bfree);
		sbp->f_bavail = fxdr_unsigned(int32_t, sfp->sf_bavail);
		sbp->f_fresvd = 0;
		sbp->f_files = 0;
		sbp->f_ffree = 0;
		sbp->f_favail = 0;
		sbp->f_fresvd = 0;
		sbp->f_namemax = NFS_MAXNAMLEN;
	}
	copy_statvfs_info(sbp, mp);
	nfsm_reqdone;
	kauth_cred_free(cred);
	return (error);
}

#ifndef NFS_V2_ONLY
/*
 * nfs version 3 fsinfo rpc call
 */
int
nfs_fsinfo(struct nfsmount *nmp, struct vnode *vp, kauth_cred_t cred, struct lwp *l)
{
	struct nfsv3_fsinfo *fsp;
	char *cp;
	int32_t t1, t2;
	u_int32_t *tl, pref, xmax;
	char *bpos, *dpos, *cp2;
	int error = 0, retattr;
	struct mbuf *mreq, *mrep, *md, *mb;
	u_int64_t maxfsize;
	struct nfsnode *np = VTONFS(vp);

	nfsstats.rpccnt[NFSPROC_FSINFO]++;
	nfsm_reqhead(np, NFSPROC_FSINFO, NFSX_FH(1));
	nfsm_fhtom(np, 1);
	nfsm_request(np, NFSPROC_FSINFO, l, cred);
	nfsm_postop_attr(vp, retattr, 0);
	if (!error) {
		nfsm_dissect(fsp, struct nfsv3_fsinfo *, NFSX_V3FSINFO);
		pref = fxdr_unsigned(u_int32_t, fsp->fs_wtpref);
		if ((nmp->nm_flag & NFSMNT_WSIZE) == 0 &&
		    pref < nmp->nm_wsize && pref >= NFS_FABLKSIZE)
			nmp->nm_wsize = (pref + NFS_FABLKSIZE - 1) &
				~(NFS_FABLKSIZE - 1);
		xmax = fxdr_unsigned(u_int32_t, fsp->fs_wtmax);
		if (xmax < nmp->nm_wsize && xmax > 0) {
			nmp->nm_wsize = xmax & ~(NFS_FABLKSIZE - 1);
			if (nmp->nm_wsize == 0)
				nmp->nm_wsize = xmax;
		}
		pref = fxdr_unsigned(u_int32_t, fsp->fs_rtpref);
		if ((nmp->nm_flag & NFSMNT_RSIZE) == 0 &&
		    pref < nmp->nm_rsize && pref >= NFS_FABLKSIZE)
			nmp->nm_rsize = (pref + NFS_FABLKSIZE - 1) &
				~(NFS_FABLKSIZE - 1);
		xmax = fxdr_unsigned(u_int32_t, fsp->fs_rtmax);
		if (xmax < nmp->nm_rsize && xmax > 0) {
			nmp->nm_rsize = xmax & ~(NFS_FABLKSIZE - 1);
			if (nmp->nm_rsize == 0)
				nmp->nm_rsize = xmax;
		}
		pref = fxdr_unsigned(u_int32_t, fsp->fs_dtpref);
		if (pref < nmp->nm_readdirsize && pref >= NFS_DIRFRAGSIZ)
			nmp->nm_readdirsize = (pref + NFS_DIRFRAGSIZ - 1) &
				~(NFS_DIRFRAGSIZ - 1);
		if (xmax < nmp->nm_readdirsize && xmax > 0) {
			nmp->nm_readdirsize = xmax & ~(NFS_DIRFRAGSIZ - 1);
			if (nmp->nm_readdirsize == 0)
				nmp->nm_readdirsize = xmax;
		}
		/* XXX */
		nmp->nm_maxfilesize = (u_int64_t)0x80000000 * DEV_BSIZE - 1;
		maxfsize = fxdr_hyper(&fsp->fs_maxfilesize);
		if (maxfsize > 0 && maxfsize < nmp->nm_maxfilesize)
			nmp->nm_maxfilesize = maxfsize;
		nmp->nm_mountp->mnt_fs_bshift =
		    ffs(MIN(nmp->nm_rsize, nmp->nm_wsize)) - 1;
		nmp->nm_iflag |= NFSMNT_GOTFSINFO;
	}
	nfsm_reqdone;
	return (error);
}
#endif

/*
 * Mount a remote root fs via. NFS.  It goes like this:
 * - Call nfs_boot_init() to fill in the nfs_diskless struct
 * - build the rootfs mount point and call mountnfs() to do the rest.
 */
int
nfs_mountroot(void)
{
	struct timespec ts;
	struct nfs_diskless *nd;
	struct vattr attr;
	struct mount *mp;
	struct vnode *vp;
	struct lwp *l;
	long n;
	int error;

	l = curlwp; /* XXX */

	if (device_class(root_device) != DV_IFNET)
		return (ENODEV);

	/*
	 * XXX time must be non-zero when we init the interface or else
	 * the arp code will wedge.  [Fixed now in if_ether.c]
	 * However, the NFS attribute cache gives false "hits" when the
	 * current time < nfs_attrtimeo(nmp, np) so keep this in for now.
	 */
	if (time_second < NFS_MAXATTRTIMO) {
		ts.tv_sec = NFS_MAXATTRTIMO;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}

	/*
	 * Call nfs_boot_init() to fill in the nfs_diskless struct.
	 * Side effect:  Finds and configures a network interface.
	 */
	nd = kmem_zalloc(sizeof(*nd), KM_SLEEP);
	error = nfs_boot_init(nd, l);
	if (error) {
		kmem_free(nd, sizeof(*nd));
		return (error);
	}

	/*
	 * Create the root mount point.
	 */
	error = nfs_mount_diskless(&nd->nd_root, "/", &mp, &vp, l);
	if (error)
		goto out;
	printf("root on %s\n", nd->nd_root.ndm_host);

	/*
	 * Link it into the mount list.
	 */
	mountlist_append(mp);
	rootvp = vp;
	mp->mnt_vnodecovered = NULLVP;
	vfs_unbusy(mp, false, NULL);

	/* Get root attributes (for the time). */
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &attr, l->l_cred);
	VOP_UNLOCK(vp);
	if (error)
		panic("nfs_mountroot: getattr for root");
	n = attr.va_atime.tv_sec;
#ifdef	DEBUG
	printf("root time: 0x%lx\n", n);
#endif
	setrootfstime(n);

out:
	if (error)
		nfs_boot_cleanup(nd, l);
	kmem_free(nd, sizeof(*nd));
	return (error);
}

/*
 * Internal version of mount system call for diskless setup.
 * Separate function because we used to call it twice.
 * (once for root and once for swap)
 */
static int
nfs_mount_diskless(struct nfs_dlmount *ndmntp, const char *mntname, struct mount **mpp, struct vnode **vpp, struct lwp *l)
	/* mntname:	 mount point name */
{
	struct mount *mp;
	struct mbuf *m;
	int error;

	vfs_rootmountalloc(MOUNT_NFS, mntname, &mp);

	mp->mnt_op = &nfs_vfsops;

	/*
	 * Historical practice expects NFS root file systems to
	 * be initially mounted r/w.
	 */
	mp->mnt_flag &= ~MNT_RDONLY;

	/* Get mbuf for server sockaddr. */
	m = m_get(M_WAIT, MT_SONAME);
	if (m == NULL)
		panic("nfs_mountroot: mget soname for %s", mntname);
	MCLAIM(m, &nfs_mowner);
	memcpy(mtod(m, void *), (void *)ndmntp->ndm_args.addr,
	      (m->m_len = ndmntp->ndm_args.addr->sa_len));

	error = mountnfs(&ndmntp->ndm_args, mp, m, mntname,
			 ndmntp->ndm_args.hostname, vpp, l);
	if (error) {
		vfs_unbusy(mp, false, NULL);
		vfs_destroy(mp);
		printf("nfs_mountroot: mount %s failed: %d\n",
		       mntname, error);
	} else
		*mpp = mp;

	return (error);
}

void
nfs_decode_args(struct nfsmount *nmp, struct nfs_args *argp, struct lwp *l)
{
	int s;
	int adjsock;
	int maxio;

	s = splsoftnet();

	/*
	 * Silently clear NFSMNT_NOCONN if it's a TCP mount, it makes
	 * no sense in that context.
	 */
	if (argp->sotype == SOCK_STREAM)
		argp->flags &= ~NFSMNT_NOCONN;

	/*
	 * Cookie translation is not needed for v2, silently ignore it.
	 */
	if ((argp->flags & (NFSMNT_XLATECOOKIE|NFSMNT_NFSV3)) ==
	    NFSMNT_XLATECOOKIE)
		argp->flags &= ~NFSMNT_XLATECOOKIE;

	/* Re-bind if rsrvd port requested and wasn't on one */
	adjsock = !(nmp->nm_flag & NFSMNT_RESVPORT)
		  && (argp->flags & NFSMNT_RESVPORT);
	/* Also re-bind if we're switching to/from a connected UDP socket */
	adjsock |= ((nmp->nm_flag & NFSMNT_NOCONN) !=
		    (argp->flags & NFSMNT_NOCONN));

	/* Update flags. */
	nmp->nm_flag = argp->flags;
	splx(s);

	if ((argp->flags & NFSMNT_TIMEO) && argp->timeo > 0) {
		nmp->nm_timeo = (argp->timeo * NFS_HZ + 5) / 10;
		if (nmp->nm_timeo < NFS_MINTIMEO)
			nmp->nm_timeo = NFS_MINTIMEO;
		else if (nmp->nm_timeo > NFS_MAXTIMEO)
			nmp->nm_timeo = NFS_MAXTIMEO;
	}

	if ((argp->flags & NFSMNT_RETRANS) && argp->retrans > 1) {
		nmp->nm_retry = argp->retrans;
		if (nmp->nm_retry > NFS_MAXREXMIT)
			nmp->nm_retry = NFS_MAXREXMIT;
	}

#ifndef NFS_V2_ONLY
	if (argp->flags & NFSMNT_NFSV3) {
		if (argp->sotype == SOCK_DGRAM)
			maxio = NFS_MAXDGRAMDATA;
		else
			maxio = NFS_MAXDATA;
	} else
#endif
		maxio = NFS_V2MAXDATA;

	if ((argp->flags & NFSMNT_WSIZE) && argp->wsize > 0) {
		int osize = nmp->nm_wsize;
		nmp->nm_wsize = argp->wsize;
		/* Round down to multiple of blocksize */
		nmp->nm_wsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_wsize <= 0)
			nmp->nm_wsize = NFS_FABLKSIZE;
		adjsock |= (nmp->nm_wsize != osize);
	}
	if (nmp->nm_wsize > maxio)
		nmp->nm_wsize = maxio;
	if (nmp->nm_wsize > MAXBSIZE)
		nmp->nm_wsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_RSIZE) && argp->rsize > 0) {
		int osize = nmp->nm_rsize;
		nmp->nm_rsize = argp->rsize;
		/* Round down to multiple of blocksize */
		nmp->nm_rsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_rsize <= 0)
			nmp->nm_rsize = NFS_FABLKSIZE;
		adjsock |= (nmp->nm_rsize != osize);
	}
	if (nmp->nm_rsize > maxio)
		nmp->nm_rsize = maxio;
	if (nmp->nm_rsize > MAXBSIZE)
		nmp->nm_rsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_READDIRSIZE) && argp->readdirsize > 0) {
		nmp->nm_readdirsize = argp->readdirsize;
		/* Round down to multiple of minimum blocksize */
		nmp->nm_readdirsize &= ~(NFS_DIRFRAGSIZ - 1);
		if (nmp->nm_readdirsize < NFS_DIRFRAGSIZ)
			nmp->nm_readdirsize = NFS_DIRFRAGSIZ;
		/* Bigger than buffer size makes no sense */
		if (nmp->nm_readdirsize > NFS_DIRBLKSIZ)
			nmp->nm_readdirsize = NFS_DIRBLKSIZ;
	} else if (argp->flags & NFSMNT_RSIZE)
		nmp->nm_readdirsize = nmp->nm_rsize;

	if (nmp->nm_readdirsize > maxio)
		nmp->nm_readdirsize = maxio;

	if ((argp->flags & NFSMNT_MAXGRPS) && argp->maxgrouplist >= 0 &&
		argp->maxgrouplist <= NFS_MAXGRPS)
		nmp->nm_numgrps = argp->maxgrouplist;
	if ((argp->flags & NFSMNT_READAHEAD) && argp->readahead >= 0 &&
		argp->readahead <= NFS_MAXRAHEAD)
		nmp->nm_readahead = argp->readahead;
	if ((argp->flags & NFSMNT_DEADTHRESH) && argp->deadthresh >= 1 &&
		argp->deadthresh <= NFS_NEVERDEAD)
		nmp->nm_deadthresh = argp->deadthresh;

	adjsock |= ((nmp->nm_sotype != argp->sotype) ||
		    (nmp->nm_soproto != argp->proto));
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;

	if (nmp->nm_so && adjsock) {
		nfs_safedisconnect(nmp);
		if (nmp->nm_sotype == SOCK_DGRAM)
			while (nfs_connect(nmp, (struct nfsreq *)0, l)) {
				printf("nfs_args: retrying connect\n");
				kpause("nfscn3", false, hz, NULL);
			}
	}
}

/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * memcpy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
int
nfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct lwp *l = curlwp;
	int error;
	struct nfs_args *args = data;
	struct mbuf *nam;
	struct nfsmount *nmp = VFSTONFS(mp);
	struct sockaddr *sa;
	struct vnode *vp;
	char *pth, *hst;
	size_t len;
	u_char *nfh;

	if (args == NULL)
		return EINVAL;
	if (*data_len < sizeof *args)
		return EINVAL;

	if (mp->mnt_flag & MNT_GETARGS) {

		if (nmp == NULL)
			return (EIO);
		if (args->addr != NULL) {
			sa = mtod(nmp->nm_nam, struct sockaddr *);
			error = copyout(sa, args->addr, sa->sa_len);
			if (error)
				return (error);
			args->addrlen = sa->sa_len;
		} else
			args->addrlen = 0;

		args->version = NFS_ARGSVERSION;
		args->sotype = nmp->nm_sotype;
		args->proto = nmp->nm_soproto;
		args->fh = NULL;
		args->fhsize = 0;
		args->flags = nmp->nm_flag;
		args->wsize = nmp->nm_wsize;
		args->rsize = nmp->nm_rsize;
		args->readdirsize = nmp->nm_readdirsize;
		args->timeo = nmp->nm_timeo;
		args->retrans = nmp->nm_retry;
		args->maxgrouplist = nmp->nm_numgrps;
		args->readahead = nmp->nm_readahead;
		args->leaseterm = 0; /* dummy */
		args->deadthresh = nmp->nm_deadthresh;
		args->hostname = NULL;
		*data_len = sizeof *args;
		return 0;
	}

	if (args->version != NFS_ARGSVERSION)
		return (EPROGMISMATCH);
	if (args->flags & (NFSMNT_NQNFS|NFSMNT_KERB))
		return (EPROGUNAVAIL);
#ifdef NFS_V2_ONLY
	if (args->flags & NFSMNT_NFSV3)
		return (EPROGMISMATCH);
#endif
	if (mp->mnt_flag & MNT_UPDATE) {
		if (nmp == NULL)
			return (EIO);
		/*
		 * When doing an update, we can't change from or to
		 * v3, or change cookie translation
		 */
		args->flags = (args->flags & ~(NFSMNT_NFSV3|NFSMNT_XLATECOOKIE)) |
		    (nmp->nm_flag & (NFSMNT_NFSV3|NFSMNT_XLATECOOKIE));
		nfs_decode_args(nmp, args, l);
		return (0);
	}
	if (args->fhsize < 0 || args->fhsize > NFSX_V3FHMAX)
		return (EINVAL);
	nfh = malloc(NFSX_V3FHMAX, M_TEMP, M_WAITOK);
	error = copyin(args->fh, nfh, args->fhsize);
	if (error)
		goto free_nfh;
	pth = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(path, pth, MNAMELEN - 1, &len);
	if (error)
		goto free_pth;
	memset(&pth[len], 0, MNAMELEN - len);
	hst = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	error = copyinstr(args->hostname, hst, MNAMELEN - 1, &len);
	if (error)
		goto free_hst;
	memset(&hst[len], 0, MNAMELEN - len);
	/* sockargs() call must be after above copyin() calls */
	error = sockargs(&nam, args->addr, args->addrlen, MT_SONAME);
	if (error)
		goto free_hst;
	MCLAIM(nam, &nfs_mowner);
	args->fh = nfh;
	error = mountnfs(args, mp, nam, pth, hst, &vp, l);

free_hst:
	free(hst, M_TEMP);
free_pth:
	free(pth, M_TEMP);
free_nfh:
	free(nfh, M_TEMP);

	return (error);
}

/*
 * Common code for mount and mountroot
 */
int
mountnfs(struct nfs_args *argp, struct mount *mp, struct mbuf *nam, const char *pth, const char *hst, struct vnode **vpp, struct lwp *l)
{
	struct nfsmount *nmp;
	struct nfsnode *np;
	struct vnode *vp;
	int error;
	struct vattr *attrs;
	kauth_cred_t cr;
	char iosname[IOSTATNAMELEN];

	/*
	 * If the number of nfs iothreads to use has never
	 * been set, create a reasonable number of them.
	 */

	if (nfs_niothreads < 0) {
		nfs_set_niothreads(NFS_DEFAULT_NIOTHREADS);
	}

	if (mp->mnt_flag & MNT_UPDATE) {
		nmp = VFSTONFS(mp);
		/* update paths, file handles, etc, here	XXX */
		m_freem(nam);
		return (0);
	} else {
		nmp = kmem_zalloc(sizeof(*nmp), KM_SLEEP);
		mp->mnt_data = nmp;
		TAILQ_INIT(&nmp->nm_uidlruhead);
		TAILQ_INIT(&nmp->nm_bufq);
		rw_init(&nmp->nm_writeverflock);
		mutex_init(&nmp->nm_lock, MUTEX_DEFAULT, IPL_NONE);
		cv_init(&nmp->nm_rcvcv, "nfsrcv");
		cv_init(&nmp->nm_sndcv, "nfssnd");
		cv_init(&nmp->nm_aiocv, "nfsaio");
		cv_init(&nmp->nm_disconcv, "nfsdis");
	}
	vfs_getnewfsid(mp);
	nmp->nm_mountp = mp;

#ifndef NFS_V2_ONLY
	if ((argp->flags & NFSMNT_NFSV3) == 0)
#endif
	{
		if (argp->fhsize != NFSX_V2FH) {
			return EINVAL;
		}
	}

	/*
	 * V2 can only handle 32 bit filesizes. For v3, nfs_fsinfo
	 * will overwrite this.
	 */
	nmp->nm_maxfilesize = 0xffffffffLL;

	nmp->nm_timeo = NFS_TIMEO;
	nmp->nm_retry = NFS_RETRANS;
	nmp->nm_wsize = NFS_WSIZE;
	nmp->nm_rsize = NFS_RSIZE;
	nmp->nm_readdirsize = NFS_READDIRSIZE;
	nmp->nm_numgrps = NFS_MAXGRPS;
	nmp->nm_readahead = NFS_DEFRAHEAD;
	nmp->nm_deadthresh = NFS_DEFDEADTHRESH;
	error = set_statvfs_info(pth, UIO_SYSSPACE, hst, UIO_SYSSPACE,
	    mp->mnt_op->vfs_name, mp, l);
	if (error)
		goto bad;
	nmp->nm_nam = nam;

	/* Set up the sockets and per-host congestion */
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;

	nfs_decode_args(nmp, argp, l);

	mp->mnt_fs_bshift = ffs(MIN(nmp->nm_rsize, nmp->nm_wsize)) - 1;
	mp->mnt_dev_bshift = DEV_BSHIFT;

	/*
	 * For Connection based sockets (TCP,...) defer the connect until
	 * the first request, in case the server is not responding.
	 */
	if (nmp->nm_sotype == SOCK_DGRAM &&
		(error = nfs_connect(nmp, (struct nfsreq *)0, l)))
		goto bad;

	/*
	 * This is silly, but it has to be set so that vinifod() works.
	 * We do not want to do an nfs_statvfs() here since we can get
	 * stuck on a dead server and we are holding a lock on the mount
	 * point.
	 */
	mp->mnt_stat.f_iosize = NFS_MAXDGRAMDATA;
	error = nfs_nget(mp, (nfsfh_t *)argp->fh, argp->fhsize, &np);
	if (error)
		goto bad;
	vp = NFSTOV(np);
	attrs = malloc(sizeof(struct vattr), M_TEMP, M_WAITOK);
	VOP_GETATTR(vp, attrs, l->l_cred);
	if ((nmp->nm_flag & NFSMNT_NFSV3) && (vp->v_type == VDIR)) {
		cr = kauth_cred_alloc();
		kauth_cred_setuid(cr, attrs->va_uid);
		kauth_cred_seteuid(cr, attrs->va_uid);
		kauth_cred_setsvuid(cr, attrs->va_uid);
		kauth_cred_setgid(cr, attrs->va_gid);
		kauth_cred_setegid(cr, attrs->va_gid);
		kauth_cred_setsvgid(cr, attrs->va_gid);
		nfs_cookieheuristic(vp, &nmp->nm_iflag, l, cr);
		kauth_cred_free(cr);
	}
	free(attrs, M_TEMP);

	/*
	 * A reference count is needed on the nfsnode representing the
	 * remote root.  If this object is not persistent, then backward
	 * traversals of the mount point (i.e. "..") will not work if
	 * the nfsnode gets flushed out of the cache. Ufs does not have
	 * this problem, because one can identify root inodes by their
	 * number == UFS_ROOTINO (2). So, just unlock, but no rele.
	 */

	nmp->nm_vnode = vp;
	if (vp->v_type == VNON)
		vp->v_type = VDIR;
	vp->v_vflag |= VV_ROOT;
	VOP_UNLOCK(vp);
	*vpp = vp;

	snprintf(iosname, sizeof(iosname), "nfs%u", nfs_mount_count++);
	nmp->nm_stats = iostat_alloc(IOSTAT_NFS, nmp, iosname);

	return (0);
bad:
	nfs_disconnect(nmp);
	rw_destroy(&nmp->nm_writeverflock);
	mutex_destroy(&nmp->nm_lock);
	cv_destroy(&nmp->nm_rcvcv);
	cv_destroy(&nmp->nm_sndcv);
	cv_destroy(&nmp->nm_aiocv);
	cv_destroy(&nmp->nm_disconcv);
	kmem_free(nmp, sizeof(*nmp));
	m_freem(nam);
	return (error);
}

/*
 * unmount system call
 */
int
nfs_unmount(struct mount *mp, int mntflags)
{
	struct nfsmount *nmp = VFSTONFS(mp);
	struct vnode *vp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE) {
		mutex_enter(&nmp->nm_lock);
		flags |= FORCECLOSE;
		nmp->nm_iflag |= NFSMNT_DISMNTFORCE;
		mutex_exit(&nmp->nm_lock);

	}

	/*
	 * Goes something like this..
	 * - Check for activity on the root vnode (other than ourselves).
	 * - Call vflush() to clear out vnodes for this file system,
	 *   except for the root vnode.
	 * - Decrement reference on the vnode representing remote root.
	 * - Close the socket
	 * - Free up the data structures
	 */
	/*
	 * We need to decrement the ref. count on the nfsnode representing
	 * the remote root.  See comment in mountnfs().
	 */
	vp = nmp->nm_vnode;
	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (error != 0)
		goto err;

	if ((mntflags & MNT_FORCE) == 0 && vp->v_usecount > 1) {
		VOP_UNLOCK(vp);
		error = EBUSY;
		goto err;
	}

	error = vflush(mp, vp, flags);
	if (error) {
		VOP_UNLOCK(vp);
		goto err;
	}

	/*
	 * We are now committed to the unmount; mark the mount structure
	 * as doomed so that any sleepers kicked awake by nfs_disconnect
	 * will go away cleanly.
	 */
	nmp->nm_iflag |= NFSMNT_DISMNT;

	/*
	 * No new async I/O will be added, but await for pending
	 * ones to drain.
	 */
	while (nfs_iodbusy(nmp))
		kpause("nfsumnt", false, hz, NULL);

	/*
	 * Clean up the stats... note that we carefully avoid decrementing
	 * nfs_mount_count here for good reason - we may not be unmounting
	 * the last thing mounted.
	 */
	iostat_free(nmp->nm_stats);

	/*
	 * There is one reference count to get rid of here
	 * (see comment in mountnfs()).
	 */
	VOP_UNLOCK(vp);
	vgone(vp);
	nfs_disconnect(nmp);
	m_freem(nmp->nm_nam);

	rw_destroy(&nmp->nm_writeverflock);
	mutex_destroy(&nmp->nm_lock);
	cv_destroy(&nmp->nm_rcvcv);
	cv_destroy(&nmp->nm_sndcv);
	cv_destroy(&nmp->nm_aiocv);
	cv_destroy(&nmp->nm_disconcv);
	kmem_free(nmp, sizeof(*nmp));
	return (0);

err:
	if (mntflags & MNT_FORCE) {
		mutex_enter(&nmp->nm_lock);
		nmp->nm_iflag &= ~NFSMNT_DISMNTFORCE;	
		mutex_exit(&nmp->nm_lock);
	}

	return error;
}

/*
 * Return root of a filesystem
 */
int
nfs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *vp;
	struct nfsmount *nmp;
	int error;

	nmp = VFSTONFS(mp);
	vp = nmp->nm_vnode;
	vref(vp);
	error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (error != 0) {
		vrele(vp);
		return error;
	}
	*vpp = vp;
	return (0);
}

extern int syncprt;

static bool
nfs_sync_selector(void *cl, struct vnode *vp)
{

	return !LIST_EMPTY(&vp->v_dirtyblkhd) || !UVM_OBJ_IS_CLEAN(&vp->v_uobj);
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
int
nfs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
	struct vnode *vp;
	struct vnode_iterator *marker;
	int error, allerror = 0;

	/*
	 * Force stale buffer cache information to be flushed.
	 */
	vfs_vnode_iterator_init(mp, &marker);
	while ((vp = vfs_vnode_iterator_next(marker, nfs_sync_selector,
	    NULL)))
	{
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error) {
			vrele(vp);
			continue;
		}
		error = VOP_FSYNC(vp, cred,
		    waitfor == MNT_WAIT ? FSYNC_WAIT : 0, 0, 0);
		if (error)
			allerror = error;
		vput(vp);
	}
	vfs_vnode_iterator_destroy(marker);
	return allerror;
}

/*
 * NFS flat namespace lookup.
 * Currently unsupported.
 */
/* ARGSUSED */
int
nfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{

	return (EOPNOTSUPP);
}

/*
 * Do that sysctl thang...
 */
static int
sysctl_vfs_nfs_iothreads(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int val;
	int error;

	val = nfs_niothreads;
	node = *rnode;
	node.sysctl_data = &val;
        error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	return nfs_set_niothreads(val);
}

static void
nfs_sysctl_init(void)
{

	sysctl_createv(&nfs_clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "nfs",
		       SYSCTL_DESCR("NFS vfs options"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, 2, CTL_EOL);
	/*
	 * XXX the "2" above could be dynamic, thereby eliminating one
	 * more instance of the "number to vfs" mapping problem, but
	 * "2" is the order as taken from sys/mount.h
	 */

	sysctl_createv(&nfs_clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_STRUCT, "nfsstats",
		       SYSCTL_DESCR("NFS operation statistics"),
		       NULL, 0, &nfsstats, sizeof(nfsstats),
		       CTL_VFS, 2, NFS_NFSSTATS, CTL_EOL);
	sysctl_createv(&nfs_clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "iothreads",
		       SYSCTL_DESCR("Number of NFS client processes desired"),
		       sysctl_vfs_nfs_iothreads, 0, NULL, 0,
		       CTL_VFS, 2, NFS_IOTHREADS, CTL_EOL);
}

static void
nfs_sysctl_fini(void)
{

	sysctl_teardown(&nfs_clog);
}

/* ARGSUSED */
int
nfs_fhtovp(struct mount *mp, struct fid *fid, struct vnode **vpp)
{
	size_t fidsize;
	size_t fhsize;
	struct nfsnode *np;
	int error;
	struct vattr va;

	fidsize = fid->fid_len;
	if (fidsize < sizeof(*fid)) {
		return EINVAL;
	}
	fhsize = fidsize - sizeof(*fid);
	if ((fhsize % NFSX_UNSIGNED) != 0) {
		return EINVAL;
	}
	if ((VFSTONFS(mp)->nm_flag & NFSMNT_NFSV3) != 0) {
		if (fhsize > NFSX_V3FHMAX || fhsize == 0) {
			return EINVAL;
		}
	} else {
		if (fhsize != NFSX_V2FH) {
			return EINVAL;
		}
	}
	error = nfs_nget(mp, (void *)fid->fid_data, fhsize, &np);
	if (error) {
		return error;
	}
	*vpp = NFSTOV(np);
	error = VOP_GETATTR(*vpp, &va, kauth_cred_get());
	if (error != 0) {
		vput(*vpp);
		*vpp = NULLVP;
	}
	return error;
}

/* ARGSUSED */
int
nfs_vptofh(struct vnode *vp, struct fid *buf, size_t *bufsize)
{
	struct nfsnode *np;
	struct fid *fid;
	size_t fidsize;
	int error = 0;

	np = VTONFS(vp);
	fidsize = sizeof(*fid) + np->n_fhsize;
	if (*bufsize < fidsize) {
		error = E2BIG;
	}
	*bufsize = fidsize;
	if (error == 0) {
		struct fid fid_store;

		fid = &fid_store;
		memset(fid, 0, sizeof(*fid));
		fid->fid_len = fidsize;
		memcpy(buf, fid, sizeof(*fid));
		memcpy(buf->fid_data, np->n_fhp, np->n_fhsize);
	}
	return error;
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
int
nfs_start(struct mount *mp, int flags)
{

	return (0);
}

/*
 * Called once at VFS init to initialize client-specific data structures.
 */
void
nfs_vfs_init(void)
{

	/* Initialize NFS server / client shared data. */
	nfs_init();
	nfs_node_init();

	/* Initialize the kqueue structures */
	nfs_kqinit();
	/* Initialize the iod structures */
	nfs_iodinit();

	nfs_commitsize = uvmexp.npages << (PAGE_SHIFT - 4);
}

void
nfs_vfs_done(void)
{

	nfs_node_done();
	nfs_kqfini();
	nfs_iodfini();
}
