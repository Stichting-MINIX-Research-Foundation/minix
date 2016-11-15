/*	$NetBSD: nfs_clntsubs.c,v 1.2 2011/06/12 03:35:59 rmind Exp $	*/

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
 *	@(#)nfs_subs.c	8.8 (Berkeley) 5/22/95
 */

/*
 * Copyright 2000 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_clntsubs.c,v 1.2 2011/06/12 03:35:59 rmind Exp $");

#ifdef _KERNEL_OPT
#include "opt_nfs.h"
#endif

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/time.h>
#include <sys/dirent.h>
#include <sys/once.h>
#include <sys/kauth.h>
#include <sys/atomic.h>

#include <uvm/uvm_extern.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsrtt.h>
#include <nfs/nfs_var.h>

#include <miscfs/specfs/specdev.h>

#include <netinet/in.h>

/*
 * Attribute cache routines.
 * nfs_loadattrcache() - loads or updates the cache contents from attributes
 *	that are on the mbuf list
 * nfs_getattrcache() - returns valid attributes if found in cache, returns
 *	error otherwise
 */

/*
 * Load the attribute cache (that lives in the nfsnode entry) with
 * the values on the mbuf list and
 * Iff vap not NULL
 *    copy the attributes to *vaper
 */
int
nfsm_loadattrcache(struct vnode **vpp, struct mbuf **mdp, char **dposp, struct vattr *vaper, int flags)
{
	int32_t t1;
	char *cp2;
	int error = 0;
	struct mbuf *md;
	int v3 = NFS_ISV3(*vpp);

	md = *mdp;
	t1 = (mtod(md, char *) + md->m_len) - *dposp;
	error = nfsm_disct(mdp, dposp, NFSX_FATTR(v3), t1, &cp2);
	if (error)
		return (error);
	return nfs_loadattrcache(vpp, (struct nfs_fattr *)cp2, vaper, flags);
}

int
nfs_loadattrcache(struct vnode **vpp, struct nfs_fattr *fp, struct vattr *vaper, int flags)
{
	struct vnode *vp = *vpp;
	struct vattr *vap;
	int v3 = NFS_ISV3(vp);
	enum vtype vtyp;
	u_short vmode;
	struct timespec mtime;
	struct timespec ctime;
	int32_t rdev;
	struct nfsnode *np;
	extern int (**spec_nfsv2nodeop_p)(void *);
	uid_t uid;
	gid_t gid;

	if (v3) {
		vtyp = nfsv3tov_type(fp->fa_type);
		vmode = fxdr_unsigned(u_short, fp->fa_mode);
		rdev = makedev(fxdr_unsigned(u_int32_t, fp->fa3_rdev.specdata1),
			fxdr_unsigned(u_int32_t, fp->fa3_rdev.specdata2));
		fxdr_nfsv3time(&fp->fa3_mtime, &mtime);
		fxdr_nfsv3time(&fp->fa3_ctime, &ctime);
	} else {
		vtyp = nfsv2tov_type(fp->fa_type);
		vmode = fxdr_unsigned(u_short, fp->fa_mode);
		if (vtyp == VNON || vtyp == VREG)
			vtyp = IFTOVT(vmode);
		rdev = fxdr_unsigned(int32_t, fp->fa2_rdev);
		fxdr_nfsv2time(&fp->fa2_mtime, &mtime);
		ctime.tv_sec = fxdr_unsigned(u_int32_t,
		    fp->fa2_ctime.nfsv2_sec);
		ctime.tv_nsec = 0;

		/*
		 * Really ugly NFSv2 kludge.
		 */
		if (vtyp == VCHR && rdev == 0xffffffff)
			vtyp = VFIFO;
	}

	vmode &= ALLPERMS;

	/*
	 * If v_type == VNON it is a new node, so fill in the v_type,
	 * n_mtime fields. Check to see if it represents a special
	 * device, and if so, check for a possible alias. Once the
	 * correct vnode has been obtained, fill in the rest of the
	 * information.
	 */
	np = VTONFS(vp);
	if (vp->v_type == VNON) {
		vp->v_type = vtyp;
		if (vp->v_type == VFIFO) {
			extern int (**fifo_nfsv2nodeop_p)(void *);
			vp->v_op = fifo_nfsv2nodeop_p;
		} else if (vp->v_type == VREG) {
			mutex_init(&np->n_commitlock, MUTEX_DEFAULT, IPL_NONE);
		} else if (vp->v_type == VCHR || vp->v_type == VBLK) {
			vp->v_op = spec_nfsv2nodeop_p;
			spec_node_init(vp, (dev_t)rdev);
		}
		np->n_mtime = mtime;
	}
	uid = fxdr_unsigned(uid_t, fp->fa_uid);
	gid = fxdr_unsigned(gid_t, fp->fa_gid);
	vap = np->n_vattr;

	/*
	 * Invalidate access cache if uid, gid, mode or ctime changed.
	 */
	if (np->n_accstamp != -1 &&
	    (gid != vap->va_gid || uid != vap->va_uid || vmode != vap->va_mode
	    || timespeccmp(&ctime, &vap->va_ctime, !=)))
		np->n_accstamp = -1;

	vap->va_type = vtyp;
	vap->va_mode = vmode;
	vap->va_rdev = (dev_t)rdev;
	vap->va_mtime = mtime;
	vap->va_ctime = ctime;
	vap->va_birthtime.tv_sec = VNOVAL;
	vap->va_birthtime.tv_nsec = VNOVAL;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsidx.__fsid_val[0];
	switch (vtyp) {
	case VDIR:
		vap->va_blocksize = NFS_DIRFRAGSIZ;
		break;
	case VBLK:
		vap->va_blocksize = BLKDEV_IOSIZE;
		break;
	case VCHR:
		vap->va_blocksize = MAXBSIZE;
		break;
	default:
		vap->va_blocksize = v3 ? vp->v_mount->mnt_stat.f_iosize :
		    fxdr_unsigned(int32_t, fp->fa2_blocksize);
		break;
	}
	if (v3) {
		vap->va_nlink = fxdr_unsigned(u_short, fp->fa_nlink);
		vap->va_uid = uid;
		vap->va_gid = gid;
		vap->va_size = fxdr_hyper(&fp->fa3_size);
		vap->va_bytes = fxdr_hyper(&fp->fa3_used);
		vap->va_fileid = fxdr_hyper(&fp->fa3_fileid);
		fxdr_nfsv3time(&fp->fa3_atime, &vap->va_atime);
		vap->va_flags = 0;
		vap->va_filerev = 0;
	} else {
		vap->va_nlink = fxdr_unsigned(u_short, fp->fa_nlink);
		vap->va_uid = uid;
		vap->va_gid = gid;
		vap->va_size = fxdr_unsigned(u_int32_t, fp->fa2_size);
		vap->va_bytes = fxdr_unsigned(int32_t, fp->fa2_blocks)
		    * NFS_FABLKSIZE;
		vap->va_fileid = fxdr_unsigned(int32_t, fp->fa2_fileid);
		fxdr_nfsv2time(&fp->fa2_atime, &vap->va_atime);
		vap->va_flags = 0;
		vap->va_gen = fxdr_unsigned(u_int32_t,fp->fa2_ctime.nfsv2_usec);
		vap->va_filerev = 0;
	}
	if (vap->va_size > VFSTONFS(vp->v_mount)->nm_maxfilesize) {
		return EFBIG;
	}
	if (vap->va_size != np->n_size) {
		if ((np->n_flag & NMODIFIED) && vap->va_size < np->n_size) {
			vap->va_size = np->n_size;
		} else {
			np->n_size = vap->va_size;
			if (vap->va_type == VREG) {
				/*
				 * we can't free pages if NAC_NOTRUNC because
				 * the pages can be owned by ourselves.
				 */
				if (flags & NAC_NOTRUNC) {
					np->n_flag |= NTRUNCDELAYED;
				} else {
					genfs_node_wrlock(vp);
					mutex_enter(vp->v_interlock);
					(void)VOP_PUTPAGES(vp, 0,
					    0, PGO_SYNCIO | PGO_CLEANIT |
					    PGO_FREE | PGO_ALLPAGES);
					uvm_vnp_setsize(vp, np->n_size);
					genfs_node_unlock(vp);
				}
			}
		}
	}
	np->n_attrstamp = time_second;
	if (vaper != NULL) {
		memcpy((void *)vaper, (void *)vap, sizeof(*vap));
		if (np->n_flag & NCHG) {
			if (np->n_flag & NACC)
				vaper->va_atime = np->n_atim;
			if (np->n_flag & NUPD)
				vaper->va_mtime = np->n_mtim;
		}
	}
	return (0);
}

/*
 * Check the time stamp
 * If the cache is valid, copy contents to *vap and return 0
 * otherwise return an error
 */
int
nfs_getattrcache(struct vnode *vp, struct vattr *vaper)
{
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct vattr *vap;

	if (np->n_attrstamp == 0 ||
	    (time_second - np->n_attrstamp) >= nfs_attrtimeo(nmp, np)) {
		nfsstats.attrcache_misses++;
		return (ENOENT);
	}
	nfsstats.attrcache_hits++;
	vap = np->n_vattr;
	if (vap->va_size != np->n_size) {
		if (vap->va_type == VREG) {
			if ((np->n_flag & NMODIFIED) != 0 &&
			    vap->va_size < np->n_size) {
				vap->va_size = np->n_size;
			} else {
				np->n_size = vap->va_size;
			}
			genfs_node_wrlock(vp);
			uvm_vnp_setsize(vp, np->n_size);
			genfs_node_unlock(vp);
		} else
			np->n_size = vap->va_size;
	}
	memcpy((void *)vaper, (void *)vap, sizeof(struct vattr));
	if (np->n_flag & NCHG) {
		if (np->n_flag & NACC)
			vaper->va_atime = np->n_atim;
		if (np->n_flag & NUPD)
			vaper->va_mtime = np->n_mtim;
	}
	return (0);
}

void
nfs_delayedtruncate(struct vnode *vp)
{
	struct nfsnode *np = VTONFS(vp);

	if (np->n_flag & NTRUNCDELAYED) {
		np->n_flag &= ~NTRUNCDELAYED;
		genfs_node_wrlock(vp);
		mutex_enter(vp->v_interlock);
		(void)VOP_PUTPAGES(vp, 0,
		    0, PGO_SYNCIO | PGO_CLEANIT | PGO_FREE | PGO_ALLPAGES);
		uvm_vnp_setsize(vp, np->n_size);
		genfs_node_unlock(vp);
	}
}

#define	NFS_WCCKLUDGE_TIMEOUT	(24 * 60 * 60)	/* 1 day */
#define	NFS_WCCKLUDGE(nmp, now) \
	(((nmp)->nm_iflag & NFSMNT_WCCKLUDGE) && \
	((now) - (nmp)->nm_wcckludgetime - NFS_WCCKLUDGE_TIMEOUT) < 0)

/*
 * nfs_check_wccdata: check inaccurate wcc_data
 *
 * => return non-zero if we shouldn't trust the wcc_data.
 * => NFS_WCCKLUDGE_TIMEOUT is for the case that the server is "fixed".
 */

int
nfs_check_wccdata(struct nfsnode *np, const struct timespec *ctime,
    struct timespec *mtime, bool docheck)
{
	int error = 0;

#if !defined(NFS_V2_ONLY)

	if (docheck) {
		struct vnode *vp = NFSTOV(np);
		struct nfsmount *nmp;
		long now = time_second;
		const struct timespec *omtime = &np->n_vattr->va_mtime;
		const struct timespec *octime = &np->n_vattr->va_ctime;
		const char *reason = NULL; /* XXX: gcc */

		if (timespeccmp(omtime, mtime, <=)) {
			reason = "mtime";
			error = EINVAL;
		}

		if (vp->v_type == VDIR && timespeccmp(octime, ctime, <=)) {
			reason = "ctime";
			error = EINVAL;
		}

		nmp = VFSTONFS(vp->v_mount);
		if (error) {

			/*
			 * despite of the fact that we've updated the file,
			 * timestamps of the file were not updated as we
			 * expected.
			 * it means that the server has incompatible
			 * semantics of timestamps or (more likely)
			 * the server time is not precise enough to
			 * track each modifications.
			 * in that case, we disable wcc processing.
			 *
			 * yes, strictly speaking, we should disable all
			 * caching.  it's a compromise.
			 */

			mutex_enter(&nmp->nm_lock);
			if (!NFS_WCCKLUDGE(nmp, now)) {
				printf("%s: inaccurate wcc data (%s) detected,"
				    " disabling wcc"
				    " (ctime %u.%09u %u.%09u,"
				    " mtime %u.%09u %u.%09u)\n",
				    vp->v_mount->mnt_stat.f_mntfromname,
				    reason,
				    (unsigned int)octime->tv_sec,
				    (unsigned int)octime->tv_nsec,
				    (unsigned int)ctime->tv_sec,
				    (unsigned int)ctime->tv_nsec,
				    (unsigned int)omtime->tv_sec,
				    (unsigned int)omtime->tv_nsec,
				    (unsigned int)mtime->tv_sec,
				    (unsigned int)mtime->tv_nsec);
			}
			nmp->nm_iflag |= NFSMNT_WCCKLUDGE;
			nmp->nm_wcckludgetime = now;
			mutex_exit(&nmp->nm_lock);
		} else if (NFS_WCCKLUDGE(nmp, now)) {
			error = EPERM; /* XXX */
		} else if (nmp->nm_iflag & NFSMNT_WCCKLUDGE) {
			mutex_enter(&nmp->nm_lock);
			if (nmp->nm_iflag & NFSMNT_WCCKLUDGE) {
				printf("%s: re-enabling wcc\n",
				    vp->v_mount->mnt_stat.f_mntfromname);
				nmp->nm_iflag &= ~NFSMNT_WCCKLUDGE;
			}
			mutex_exit(&nmp->nm_lock);
		}
	}

#endif /* !defined(NFS_V2_ONLY) */

	return error;
}

/*
 * Heuristic to see if the server XDR encodes directory cookies or not.
 * it is not supposed to, but a lot of servers may do this. Also, since
 * most/all servers will implement V2 as well, it is expected that they
 * may return just 32 bits worth of cookie information, so we need to
 * find out in which 32 bits this information is available. We do this
 * to avoid trouble with emulated binaries that can't handle 64 bit
 * directory offsets.
 */

void
nfs_cookieheuristic(struct vnode *vp, int *flagp, struct lwp *l, kauth_cred_t cred)
{
	struct uio auio;
	struct iovec aiov;
	char *tbuf, *cp;
	struct dirent *dp;
	off_t *cookies = NULL, *cop;
	int error, eof, nc, len;

	tbuf = malloc(NFS_DIRFRAGSIZ, M_TEMP, M_WAITOK);

	aiov.iov_base = tbuf;
	aiov.iov_len = NFS_DIRFRAGSIZ;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = NFS_DIRFRAGSIZ;
	auio.uio_offset = 0;
	UIO_SETUP_SYSSPACE(&auio);

	error = VOP_READDIR(vp, &auio, cred, &eof, &cookies, &nc);

	len = NFS_DIRFRAGSIZ - auio.uio_resid;
	if (error || len == 0) {
		free(tbuf, M_TEMP);
		if (cookies)
			free(cookies, M_TEMP);
		return;
	}

	/*
	 * Find the first valid entry and look at its offset cookie.
	 */

	cp = tbuf;
	for (cop = cookies; len > 0; len -= dp->d_reclen) {
		dp = (struct dirent *)cp;
		if (dp->d_fileno != 0 && len >= dp->d_reclen) {
			if ((*cop >> 32) != 0 && (*cop & 0xffffffffLL) == 0) {
				*flagp |= NFSMNT_SWAPCOOKIE;
				nfs_invaldircache(vp, 0);
				nfs_vinvalbuf(vp, 0, cred, l, 1);
			}
			break;
		}
		cop++;
		cp += dp->d_reclen;
	}

	free(tbuf, M_TEMP);
	free(cookies, M_TEMP);
}

/*
 * Set the attribute timeout based on how recently the file has been modified.
 */

time_t
nfs_attrtimeo(struct nfsmount *nmp, struct nfsnode *np)
{
	time_t timeo;

	if ((nmp->nm_flag & NFSMNT_NOAC) != 0)
		return 0;

	if (((np)->n_flag & NMODIFIED) != 0)
		return NFS_MINATTRTIMO;

	timeo = (time_second - np->n_mtime.tv_sec) / 10;
	timeo = max(timeo, NFS_MINATTRTIMO);
	timeo = min(timeo, NFS_MAXATTRTIMO);
	return timeo;
}
