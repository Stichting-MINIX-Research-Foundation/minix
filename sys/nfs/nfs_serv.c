/*	$NetBSD: nfs_serv.c,v 1.172 2015/04/21 03:19:03 riastradh Exp $	*/

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
 *	@(#)nfs_serv.c	8.8 (Berkeley) 7/31/95
 */

/*
 * nfs version 2 and 3 server calls to vnode ops
 * - these routines generally have 3 phases
 *   1 - break down and validate rpc request in mbuf list
 *   2 - do the vnode ops for the request
 *       (surprisingly ?? many are very similar to syscalls in vfs_syscalls.c)
 *   3 - build the rpc reply in an mbuf list
 *   nb:
 *	- do not mix the phases, since the nfsm_?? macros can return failures
 *	  on a bad rpc or similar and do not do any vrele() or vput()'s
 *
 *      - the nfsm_reply() macro generates an nfs rpc reply with the nfs
 *	error number iff error != 0 whereas
 *	returning an error from the server function implies a fatal error
 *	such as a badly constructed rpc request that should be dropped without
 *	a reply.
 *	For Version 3, nfsm_reply() does not return for the error case, since
 *	most version 3 rpcs return more than the status for error cases.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_serv.c,v 1.172 2015/04/21 03:19:03 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/hash.h>
#include <sys/kauth.h>
#include <sys/module.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/syscallvar.h>

#include <uvm/uvm.h>

#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_var.h>

MODULE(MODULE_CLASS_MISC, nfsserver, "nfs");

/* Global vars */
extern u_int32_t nfs_xdrneg1;
extern u_int32_t nfs_false, nfs_true;
extern const enum vtype nv3tov_type[8];
extern struct nfsstats nfsstats;
extern const nfstype nfsv2_type[9];
extern const nfstype nfsv3_type[9];
int nfsrvw_procrastinate = NFS_GATHERDELAY * 1000;
bool nfsd_use_loan = true;	/* use page-loan for READ OP */

#define	nqsrv_getl(vp, rw)	/* nothing */

static const struct syscall_package nfsserver_syscalls[] = {
	{ SYS_nfssvc, 0, (sy_call_t *)sys_nfssvc },
	{ 0, 0, NULL },
};

static int
nfsserver_modcmd(modcmd_t cmd, void *arg)
{
	extern struct vfs_hooks nfs_export_hooks;	/* XXX */
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = syscall_establish(NULL, nfsserver_syscalls);
		if (error != 0) {
			return error;
		}
		nfs_init();	/* XXX for monolithic kernel */
		netexport_init();
		nfsrv_initcache();	/* Init the server request cache */
		nfsrv_init(0);		/* Init server data structures */
		vfs_hooks_attach(&nfs_export_hooks);
		nfs_timer_srvinit(nfsrv_timer);
		return 0;
	case MODULE_CMD_FINI:
		error = syscall_disestablish(NULL, nfsserver_syscalls);
		if (error != 0) {
			return error;
		}
		/*
		 * Kill export list before detaching VFS hooks, so we
		 * we don't leak state due to a concurrent umount().
		 */
		netexport_fini();
		vfs_hooks_detach(&nfs_export_hooks);

		/* Kill timer before server goes away. */
		nfs_timer_srvfini();
		nfsrv_fini();

		/* Server uses server cache, so kill cache last. */
		nfsrv_finicache();
		nfs_fini();
		return 0;
	case MODULE_CMD_AUTOUNLOAD:
		if (netexport_hasexports())
			return EBUSY;
		/*FALLTHROUGH*/
	default:
		return ENOTTY;
	}
}

/*
 * nfs v3 access service
 */
int
nfsrv3_access(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct vnode *vp;
	nfsrvfh_t nsfh;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, getret;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vattr va;
	u_long inmode, testmode, outmode;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam, &rdonly,
	    (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		return (0);
	}
	inmode = fxdr_unsigned(u_int32_t, *tl);
	outmode = 0;
	if ((inmode & NFSV3ACCESS_READ) &&
	    nfsrv_access(vp, VREAD, cred, rdonly, lwp, 0) == 0)
		outmode |= NFSV3ACCESS_READ;
	if (vp->v_type != VDIR) {
		testmode = inmode & (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
		if (testmode &&
		    nfsrv_access(vp, VWRITE, cred, rdonly, lwp, 0) == 0)
			outmode |= testmode;
		if ((inmode & NFSV3ACCESS_EXECUTE) &&
		    nfsrv_access(vp, VEXEC, cred, rdonly, lwp, 0) == 0)
			outmode |= NFSV3ACCESS_EXECUTE;
	} else {
		testmode = inmode & (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
		    NFSV3ACCESS_DELETE);
		if (testmode &&
		    nfsrv_access(vp, VWRITE, cred, rdonly, lwp, 0) == 0)
			outmode |= testmode;
		if ((inmode & NFSV3ACCESS_LOOKUP) &&
		    nfsrv_access(vp, VEXEC, cred, rdonly, lwp, 0) == 0)
			outmode |= NFSV3ACCESS_LOOKUP;
	}
	getret = VOP_GETATTR(vp, &va, cred);
	vput(vp);
	nfsm_reply(NFSX_POSTOPATTR(1) + NFSX_UNSIGNED);
	nfsm_srvpostop_attr(getret, &va);
	nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(outmode);
	nfsm_srvdone;
}

/*
 * nfs getattr service
 */
int
nfsrv_getattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va;
	struct vnode *vp;
	nfsrvfh_t nsfh;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam, &rdonly,
	    (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(0);
		return (0);
	}
	nqsrv_getl(vp, ND_READ);
	error = VOP_GETATTR(vp, &va, cred);
	vput(vp);
	nfsm_reply(NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	if (error)
		return (0);
	nfsm_build(fp, struct nfs_fattr *, NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	nfsm_srvfillattr(&va, fp);
	nfsm_srvdone;
}

/*
 * nfs setattr service
 */
int
nfsrv_setattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct vattr va, preat;
	struct nfsv2_sattr *sp;
	struct nfs_fattr *fp;
	struct vnode *vp;
	nfsrvfh_t nsfh;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, preat_ret = 1, postat_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), gcheck = 0;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	u_quad_t frev;
	struct timespec guard;

	memset(&guard, 0, sizeof guard);	/* XXX gcc */

	nfsm_srvmtofh(&nsfh);
	vattr_null(&va);
	if (v3) {
		nfsm_srvsattr(&va);
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		gcheck = fxdr_unsigned(int, *tl);
		if (gcheck) {
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &guard);
		}
	} else {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		/*
		 * Nah nah nah nah na nah
		 * There is a bug in the Sun client that puts 0xffff in the mode
		 * field of sattr when it should put in 0xffffffff. The u_short
		 * doesn't sign extend.
		 * --> check the low order 2 bytes for 0xffff
		 */
		if ((fxdr_unsigned(int, sp->sa_mode) & 0xffff) != 0xffff)
			va.va_mode = nfstov_mode(sp->sa_mode);
		if (sp->sa_uid != nfs_xdrneg1)
			va.va_uid = fxdr_unsigned(uid_t, sp->sa_uid);
		if (sp->sa_gid != nfs_xdrneg1)
			va.va_gid = fxdr_unsigned(gid_t, sp->sa_gid);
		if (sp->sa_size != nfs_xdrneg1)
			va.va_size = fxdr_unsigned(u_quad_t, sp->sa_size);
		if (sp->sa_atime.nfsv2_sec != nfs_xdrneg1) {
#ifdef notyet
			fxdr_nfsv2time(&sp->sa_atime, &va.va_atime);
#else
			va.va_atime.tv_sec =
				fxdr_unsigned(u_int32_t,sp->sa_atime.nfsv2_sec);
			va.va_atime.tv_nsec = 0;
#endif
		}
		if (sp->sa_mtime.nfsv2_sec != nfs_xdrneg1)
			fxdr_nfsv2time(&sp->sa_mtime, &va.va_mtime);

	}

	/*
	 * Now that we have all the fields, lets do it.
	 */
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam, &rdonly,
	    (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(preat_ret, &preat, postat_ret, &va);
		return (0);
	}
	nqsrv_getl(vp, ND_WRITE);
	if (v3) {
		error = preat_ret = VOP_GETATTR(vp, &preat, cred);
		if (!error && gcheck &&
			(preat.va_ctime.tv_sec != guard.tv_sec ||
			 preat.va_ctime.tv_nsec != guard.tv_nsec))
			error = NFSERR_NOT_SYNC;
		if (error) {
			vput(vp);
			nfsm_reply(NFSX_WCCDATA(v3));
			nfsm_srvwcc_data(preat_ret, &preat, postat_ret, &va);
			return (0);
		}
	}

	/*
	 * If the size is being changed write acces is required, otherwise
	 * just check for a read only file system.
	 */
	if (va.va_size == ((u_quad_t)((quad_t) -1))) {
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			error = EROFS;
			goto out;
		}
	} else {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		} else if ((error = nfsrv_access(vp, VWRITE, cred, rdonly,
			lwp, 0)) != 0)
			goto out;
	}
	error = VOP_SETATTR(vp, &va, cred);
	postat_ret = VOP_GETATTR(vp, &va, cred);
	if (!error)
		error = postat_ret;
out:
	vput(vp);
	nfsm_reply(NFSX_WCCORFATTR(v3));
	if (v3) {
		nfsm_srvwcc_data(preat_ret, &preat, postat_ret, &va);
		return (0);
	} else {
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(&va, fp);
	}
	nfsm_srvdone;
}

/*
 * nfs lookup rpc
 */
int
nfsrv_lookup(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct nameidata nd, ind, *ndp = &nd;
	struct pathbuf *ipb = NULL;
	struct vnode *vp, *dirp;
	nfsrvfh_t nsfh;
	char *cp;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, dirattr_ret = 1;
	uint32_t len;
	int v3 = (nfsd->nd_flag & ND_NFSV3), pubflag;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vattr va, dirattr;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);

	pubflag = nfs_ispublicfh(&nsfh);

	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), pubflag);

	if (!error && pubflag) {
		if (nd.ni_vp->v_type == VDIR && nfs_pub.np_index != NULL &&
		    (ipb = pathbuf_create(nfs_pub.np_index)) != NULL) {
			/*
			 * Setup call to lookup() to see if we can find
			 * the index file. Arguably, this doesn't belong
			 * in a kernel.. Ugh.
			 */
			ind = nd;
			VOP_UNLOCK(nd.ni_vp);
			ind.ni_pathbuf = ipb;

			error = lookup_for_nfsd_index(&ind, nd.ni_vp);
			if (!error) {
				/*
				 * Found an index file. Get rid of
				 * the old references.
				 */
				if (dirp)
					vrele(dirp);
				dirp = nd.ni_vp;
				ndp = &ind;
			} else
				error = 0;
		}
		/*
		 * If the public filehandle was used, check that this lookup
		 * didn't result in a filehandle outside the publicly exported
		 * filesystem.
		 */

		if (!error && ndp->ni_vp->v_mount != nfs_pub.np_mount) {
			vput(nd.ni_vp);
			error = EPERM;
		}
	}

	if (error) {
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
		}
		if (ipb != NULL) {
			pathbuf_destroy(ipb);
		}
		if (dirp) {
			if (v3) {
				vn_lock(dirp, LK_SHARED | LK_RETRY);
				dirattr_ret = VOP_GETATTR(dirp, &dirattr, cred);
				vput(dirp);
			} else
				vrele(dirp);
		}
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		return (0);
	}

	nqsrv_getl(ndp->ni_startdir, ND_READ);
	pathbuf_destroy(nd.ni_pathbuf);
	if (ipb != NULL) {
		pathbuf_destroy(ipb);
	}
	vp = ndp->ni_vp;
	error = nfsrv_composefh(vp, &nsfh, v3);
	if (!error)
		error = VOP_GETATTR(vp, &va, cred);
	vput(vp);
	if (dirp) {
		if (v3) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			dirattr_ret = VOP_GETATTR(dirp, &dirattr, cred);
			vput(dirp);
		} else
			vrele(dirp);
	}
	nfsm_reply(NFSX_SRVFH(&nsfh, v3) + NFSX_POSTOPORFATTR(v3) +
	    NFSX_POSTOPATTR(v3));
	if (error) {
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		return (0);
	}
	nfsm_srvfhtom(&nsfh, v3);
	if (v3) {
		nfsm_srvpostop_attr(0, &va);
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
	} else {
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(&va, fp);
	}
	nfsm_srvdone;
}

/*
 * nfs readlink service
 */
int
nfsrv_readlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct iovec iv[(NFS_MAXPATHLEN+MLEN-1)/MLEN];
	struct iovec *ivp = iv;
	struct mbuf *mp;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, i, padlen, getret;
	uint32_t len;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mp2 = NULL, *mp3 = NULL, *mreq __unused;
	struct vnode *vp;
	struct vattr attr;
	nfsrvfh_t nsfh;
	struct uio io, *uiop = &io;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	len = 0;
	i = 0;
	while (len < NFS_MAXPATHLEN) {
		mp = m_get(M_WAIT, MT_DATA);
		MCLAIM(mp, &nfs_mowner);
		m_clget(mp, M_WAIT);
		mp->m_len = NFSMSIZ(mp);
		if (len == 0)
			mp3 = mp2 = mp;
		else {
			mp2->m_next = mp;
			mp2 = mp;
		}
		if ((len+mp->m_len) > NFS_MAXPATHLEN) {
			mp->m_len = NFS_MAXPATHLEN-len;
			len = NFS_MAXPATHLEN;
		} else
			len += mp->m_len;
		ivp->iov_base = mtod(mp, void *);
		ivp->iov_len = mp->m_len;
		i++;
		ivp++;
	}
	uiop->uio_iov = iv;
	uiop->uio_iovcnt = i;
	uiop->uio_offset = 0;
	uiop->uio_resid = len;
	uiop->uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(uiop);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		m_freem(mp3);
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		return (0);
	}
	if (vp->v_type != VLNK) {
		if (v3)
			error = EINVAL;
		else
			error = ENXIO;
		goto out;
	}
	nqsrv_getl(vp, ND_READ);
	error = VOP_READLINK(vp, uiop, cred);
out:
	getret = VOP_GETATTR(vp, &attr, cred);
	vput(vp);
	if (error)
		m_freem(mp3);
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_UNSIGNED);
	if (v3) {
		nfsm_srvpostop_attr(getret, &attr);
		if (error)
			return (0);
	}
	len -= uiop->uio_resid;
	padlen = nfsm_padlen(len);
	if (uiop->uio_resid || padlen)
		nfs_zeropad(mp3, uiop->uio_resid, padlen);
	nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	mb->m_next = mp3;
	nfsm_srvdone;
}

/*
 * nfs read service
 */
int
nfsrv_read(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct mbuf *m;
	struct nfs_fattr *fp;
	u_int32_t *tl;
	int32_t t1;
	int i;
	char *bpos;
	int error = 0, rdonly, cache = 0, getret;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	uint32_t reqlen, len, cnt, left;
	int padlen;
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *vp;
	nfsrvfh_t nsfh;
	struct uio io, *uiop = &io;
	struct vattr va;
	off_t off;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
	} else {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *tl);
	}
	nfsm_dissect(tl, uint32_t *, NFSX_UNSIGNED);
	reqlen = fxdr_unsigned(uint32_t, *tl);
	reqlen = MIN(reqlen, NFS_SRVMAXDATA(nfsd));
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		return (0);
	}
	if (vp->v_type != VREG) {
		if (v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error) {
	    nqsrv_getl(vp, ND_READ);
	    if ((error = nfsrv_access(vp, VREAD, cred, rdonly, lwp, 1)) != 0)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, lwp, 1);
	}
	getret = VOP_GETATTR(vp, &va, cred);
	if (!error)
		error = getret;
	if (error) {
		vput(vp);
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(getret, &va);
		return (0);
	}
	if (off >= va.va_size)
		cnt = 0;
	else if ((off + reqlen) > va.va_size)
		cnt = va.va_size - off;
	else
		cnt = reqlen;
	nfsm_reply(NFSX_POSTOPORFATTR(v3) + 3 * NFSX_UNSIGNED+nfsm_rndup(cnt));
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_V3FATTR + 4 * NFSX_UNSIGNED);
		*tl++ = nfs_true;
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V3FATTR / sizeof (u_int32_t));
	} else {
		nfsm_build(tl, u_int32_t *, NFSX_V2FATTR + NFSX_UNSIGNED);
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V2FATTR / sizeof (u_int32_t));
	}
	len = left = cnt;
	if (cnt > 0) {
		if (nfsd_use_loan) {
			struct vm_page **pgpp;
			voff_t pgoff = trunc_page(off);
			int npages;
			vaddr_t lva;

			npages = (round_page(off + cnt) - pgoff) >> PAGE_SHIFT;
			KASSERT(npages <= M_EXT_MAXPAGES); /* XXX */

			/* allocate kva for mbuf data */
			lva = sokvaalloc(pgoff, npages << PAGE_SHIFT,
			    slp->ns_so);
			if (lva == 0) {
				/* fall back to VOP_READ */
				goto loan_fail;
			}

			/* allocate mbuf */
			m = m_get(M_WAIT, MT_DATA);
			MCLAIM(m, &nfs_mowner);
			pgpp = m->m_ext.ext_pgs;

			/* loan pages */
			error = uvm_loanuobjpages(&vp->v_uobj, pgoff, npages,
			    pgpp);
			if (error) {
				sokvafree(lva, npages << PAGE_SHIFT);
				m_free(m);
				if (error == EBUSY)
					goto loan_fail;
				goto read_error;
			}

			/* associate kva to mbuf */
			MEXTADD(m, (void *)(lva + ((vaddr_t)off & PAGE_MASK)),
			    cnt, M_MBUF, soloanfree, slp->ns_so);
			m->m_flags |= M_EXT_PAGES | M_EXT_ROMAP;
			m->m_len = cnt;

			/* map pages */
			for (i = 0; i < npages; i++) {
				pmap_kenter_pa(lva, VM_PAGE_TO_PHYS(pgpp[i]),
				    VM_PROT_READ, 0);
				lva += PAGE_SIZE;
			}

			pmap_update(pmap_kernel());

			mb->m_next = m;
			mb = m;
			error = 0;
			uiop->uio_resid = 0;
		} else {
			struct iovec *iv;
			struct iovec *iv2;
			struct mbuf *m2;
			int siz;
loan_fail:
			/*
			 * Generate the mbuf list with the uio_iov ref. to it.
			 */
			i = 0;
			m = m2 = mb;
			while (left > 0) {
				siz = min(M_TRAILINGSPACE(m), left);
				if (siz > 0) {
					left -= siz;
					i++;
				}
				if (left > 0) {
					m = m_get(M_WAIT, MT_DATA);
					MCLAIM(m, &nfs_mowner);
					m_clget(m, M_WAIT);
					m->m_len = 0;
					m2->m_next = m;
					m2 = m;
				}
			}
			iv = malloc(i * sizeof(struct iovec), M_TEMP, M_WAITOK);
			uiop->uio_iov = iv2 = iv;
			m = mb;
			left = cnt;
			i = 0;
			while (left > 0) {
				if (m == NULL)
					panic("nfsrv_read iov");
				siz = min(M_TRAILINGSPACE(m), left);
				if (siz > 0) {
					iv->iov_base = mtod(m, char *) +
					    m->m_len;
					iv->iov_len = siz;
					m->m_len += siz;
					left -= siz;
					iv++;
					i++;
				}
				m = m->m_next;
			}
			uiop->uio_iovcnt = i;
			uiop->uio_offset = off;
			uiop->uio_resid = cnt;
			uiop->uio_rw = UIO_READ;
			UIO_SETUP_SYSSPACE(uiop);
			error = VOP_READ(vp, uiop, IO_NODELOCKED, cred);
			free((void *)iv2, M_TEMP);
		}
read_error:
		if (error || (getret = VOP_GETATTR(vp, &va, cred)) != 0){
			if (!error)
				error = getret;
			m_freem(mreq);
			vput(vp);
			nfsm_reply(NFSX_POSTOPATTR(v3));
			nfsm_srvpostop_attr(getret, &va);
			return (0);
		}
	} else {
		uiop->uio_resid = 0;
	}
	vput(vp);
	nfsm_srvfillattr(&va, fp);
	len -= uiop->uio_resid;
	padlen = nfsm_padlen(len);
	if (uiop->uio_resid || padlen)
		nfs_zeropad(mb, uiop->uio_resid, padlen);
	if (v3) {
		/* count */
		*tl++ = txdr_unsigned(len);
		/* eof */
		if (off + len >= va.va_size)
			*tl++ = nfs_true;
		else
			*tl++ = nfs_false;
	}
	*tl = txdr_unsigned(len);
	nfsm_srvdone;
}

/*
 * nfs write service
 */
int
nfsrv_write(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct iovec *ivp;
	int i, cnt;
	struct mbuf *mp;
	struct nfs_fattr *fp;
	struct iovec *iv;
	struct vattr va, forat;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, len, forat_ret = 1;
	int ioflags, aftat_ret = 1, retlen, zeroing, adjust;
	int stable = NFSV3WRITE_FILESYNC;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp;
	nfsrvfh_t nsfh;
	struct uio io, *uiop = &io;
	off_t off;
	u_quad_t frev;

	if (mrep == NULL) {
		*mrq = NULL;
		return (0);
	}
	nfsm_srvmtofh(&nsfh);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 3;
		stable = fxdr_unsigned(int, *tl++);
	} else {
		nfsm_dissect(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
	}
	retlen = len = fxdr_unsigned(int32_t, *tl);
	cnt = i = 0;

	/*
	 * For NFS Version 2, it is not obvious what a write of zero length
	 * should do, but I might as well be consistent with Version 3,
	 * which is to return ok so long as there are no permission problems.
	 */
	if (len > 0) {
		zeroing = 1;
		mp = mrep;
		while (mp) {
			if (mp == md) {
				zeroing = 0;
				adjust = dpos - mtod(mp, char *);
				mp->m_len -= adjust;
				if (mp->m_len > 0 && adjust > 0)
					NFSMADV(mp, adjust);
			}
			if (zeroing)
				mp->m_len = 0;
			else if (mp->m_len > 0) {
				i += mp->m_len;
				if (i > len) {
					mp->m_len -= (i - len);
					zeroing	= 1;
				}
				if (mp->m_len > 0)
					cnt++;
			}
			mp = mp->m_next;
		}
	}
	if (len > NFS_MAXDATA || len < 0 || i < len) {
		error = EIO;
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		return (0);
	}
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		return (0);
	}
	if (v3)
		forat_ret = VOP_GETATTR(vp, &forat, cred);
	if (vp->v_type != VREG) {
		if (v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error) {
		nqsrv_getl(vp, ND_WRITE);
		error = nfsrv_access(vp, VWRITE, cred, rdonly, lwp, 1);
	}
	if (error) {
		vput(vp);
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		return (0);
	}

	if (len > 0) {
		ivp = malloc(cnt * sizeof (struct iovec), M_TEMP, M_WAITOK);
		uiop->uio_iov = iv = ivp;
		uiop->uio_iovcnt = cnt;
		mp = mrep;
		while (mp) {
			if (mp->m_len > 0) {
				ivp->iov_base = mtod(mp, void *);
				ivp->iov_len = mp->m_len;
				ivp++;
			}
			mp = mp->m_next;
		}

		/*
		 * XXX
		 * The IO_METASYNC flag indicates that all metadata (and not
		 * just enough to ensure data integrity) must be written to
		 * stable storage synchronously.
		 * (IO_METASYNC is not yet implemented in 4.4BSD-Lite.)
		 */
		if (stable == NFSV3WRITE_UNSTABLE)
			ioflags = IO_NODELOCKED;
		else if (stable == NFSV3WRITE_DATASYNC)
			ioflags = (IO_SYNC | IO_NODELOCKED);
		else
			ioflags = (IO_METASYNC | IO_SYNC | IO_NODELOCKED);
		uiop->uio_resid = len;
		uiop->uio_rw = UIO_WRITE;
		uiop->uio_offset = off;
		UIO_SETUP_SYSSPACE(uiop);
		error = VOP_WRITE(vp, uiop, ioflags, cred);
		nfsstats.srvvop_writes++;
		free(iv, M_TEMP);
	}
	aftat_ret = VOP_GETATTR(vp, &va, cred);
	vput(vp);
	if (!error)
		error = aftat_ret;
	nfsm_reply(NFSX_PREOPATTR(v3) + NFSX_POSTOPORFATTR(v3) +
		2 * NFSX_UNSIGNED + NFSX_WRITEVERF(v3));
	if (v3) {
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		if (error)
			return (0);
		nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(retlen);
		if (stable == NFSV3WRITE_UNSTABLE)
			*tl++ = txdr_unsigned(stable);
		else
			*tl++ = txdr_unsigned(NFSV3WRITE_FILESYNC);
		/*
		 * Actually, there is no need to txdr these fields,
		 * but it may make the values more human readable,
		 * for debugging purposes.
		 */
		*tl++ = txdr_unsigned(boottime.tv_sec);
		*tl = txdr_unsigned(boottime.tv_nsec / 1000);
	} else {
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(&va, fp);
	}
	nfsm_srvdone;
}

/*
 * XXX elad: the original NFSW_SAMECRED() macro also made sure the
 *	     two nd_flag fields of the descriptors contained
 *	     ND_KERBAUTH.
 */
static int
nfsrv_samecred(kauth_cred_t cred1, kauth_cred_t cred2)
{
	int i, do_ngroups;

	if (kauth_cred_geteuid(cred1) != kauth_cred_geteuid(cred2))
		return (0);
	if (kauth_cred_ngroups(cred1) != kauth_cred_ngroups(cred2))
		return (0);
	do_ngroups = kauth_cred_ngroups(cred1);
	for (i = 0; i < do_ngroups; i++)
		if (kauth_cred_group(cred1, i) !=
		    kauth_cred_group(cred2, i))
			return (0);

	return (1);
}

static struct nfsrvw_delayhash *
nfsrv_nwdelayhash(struct nfssvc_sock *slp, const nfsrvfh_t *nsfh)
{
	uint32_t hash;

	hash = hash32_buf(NFSRVFH_DATA(nsfh), NFSRVFH_SIZE(nsfh),
	    HASH32_BUF_INIT);
	return &slp->ns_wdelayhashtbl[hash % NFS_WDELAYHASHSIZ];
}

/*
 * NFS write service with write gathering support. Called when
 * nfsrvw_procrastinate > 0.
 * See: Chet Juszczak, "Improving the Write Performance of an NFS Server",
 * in Proc. of the Winter 1994 Usenix Conference, pg. 247-259, San Franscisco,
 * Jan. 1994.
 */
int
nfsrv_writegather(struct nfsrv_descript **ndp, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct timeval now;
	struct iovec *ivp;
	struct mbuf *mp;
	struct nfsrv_descript *wp, *nfsd, *owp, *swp;
	struct nfs_fattr *fp;
	int i = 0;
	struct iovec *iov;
	struct nfsrvw_delayhash *wpp;
	kauth_cred_t cred;
	struct vattr va, forat;
	u_int32_t *tl;
	int32_t t1;
	char *bpos, *dpos;
	int error = 0, rdonly, cache = 0, len = 0, forat_ret = 1;
	int ioflags, aftat_ret = 1, adjust, v3, zeroing;
	char *cp2;
	struct mbuf *mb, *mreq, *mrep, *md;
	struct vnode *vp;
	struct uio io, *uiop = &io;
	u_quad_t frev, cur_usec;

	*mrq = NULL;
	if (*ndp) {
	    nfsd = *ndp;
	    *ndp = NULL;
	    mrep = nfsd->nd_mrep;
	    md = nfsd->nd_md;
	    dpos = nfsd->nd_dpos;
	    cred = nfsd->nd_cr;
	    v3 = (nfsd->nd_flag & ND_NFSV3);
	    LIST_INIT(&nfsd->nd_coalesce);
	    nfsd->nd_mreq = NULL;
	    nfsd->nd_stable = NFSV3WRITE_FILESYNC;
	    getmicrotime(&now);
	    cur_usec = (u_quad_t)now.tv_sec * 1000000 + (u_quad_t)now.tv_usec;
	    nfsd->nd_time = cur_usec + nfsrvw_procrastinate;

	    /*
	     * Now, get the write header..
	     */
	    nfsm_srvmtofh(&nfsd->nd_fh);
	    if (v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		nfsd->nd_off = fxdr_hyper(tl);
		tl += 3;
		nfsd->nd_stable = fxdr_unsigned(int, *tl++);
	    } else {
		nfsm_dissect(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		nfsd->nd_off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
	    }
	    len = fxdr_unsigned(int32_t, *tl);
	    nfsd->nd_len = len;
	    nfsd->nd_eoff = nfsd->nd_off + len;

	    /*
	     * Trim the header out of the mbuf list and trim off any trailing
	     * junk so that the mbuf list has only the write data.
	     */
	    zeroing = 1;
	    i = 0;
	    mp = mrep;
	    while (mp) {
		if (mp == md) {
		    zeroing = 0;
		    adjust = dpos - mtod(mp, char *);
		    mp->m_len -= adjust;
		    if (mp->m_len > 0 && adjust > 0)
			NFSMADV(mp, adjust);
		}
		if (zeroing)
		    mp->m_len = 0;
		else {
		    i += mp->m_len;
		    if (i > len) {
			mp->m_len -= (i - len);
			zeroing = 1;
		    }
		}
		mp = mp->m_next;
	    }
	    if (len > NFS_MAXDATA || len < 0  || i < len) {
nfsmout:
		m_freem(mrep);
		error = EIO;
		nfsm_writereply(2 * NFSX_UNSIGNED, v3);
		if (v3)
		    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		nfsd->nd_mreq = mreq;
		nfsd->nd_mrep = NULL;
		nfsd->nd_time = 0;
	    }

	    /*
	     * Add this entry to the hash and time queues.
	     */
	    owp = NULL;
	    mutex_enter(&nfsd_lock);
	    wp = LIST_FIRST(&slp->ns_tq);
	    while (wp && wp->nd_time < nfsd->nd_time) {
		owp = wp;
		wp = LIST_NEXT(wp, nd_tq);
	    }
	    if (owp) {
		LIST_INSERT_AFTER(owp, nfsd, nd_tq);
	    } else {
		LIST_INSERT_HEAD(&slp->ns_tq, nfsd, nd_tq);
	    }
	    if (nfsd->nd_mrep) {
		wpp = nfsrv_nwdelayhash(slp, &nfsd->nd_fh);
		owp = NULL;
		wp = LIST_FIRST(wpp);
		while (wp && nfsrv_comparefh(&nfsd->nd_fh, &wp->nd_fh)) {
		    owp = wp;
		    wp = LIST_NEXT(wp, nd_hash);
		}
		while (wp && wp->nd_off < nfsd->nd_off &&
		    !nfsrv_comparefh(&nfsd->nd_fh, &wp->nd_fh)) {
		    owp = wp;
		    wp = LIST_NEXT(wp, nd_hash);
		}
		if (owp) {
		    LIST_INSERT_AFTER(owp, nfsd, nd_hash);

		    /*
		     * Search the hash list for overlapping entries and
		     * coalesce.
		     */
		    for(; nfsd && NFSW_CONTIG(owp, nfsd); nfsd = wp) {
			wp = LIST_NEXT(nfsd, nd_hash);
			if (nfsrv_samecred(owp->nd_cr, nfsd->nd_cr))
			    nfsrvw_coalesce(owp, nfsd);
		    }
		} else {
		    LIST_INSERT_HEAD(wpp, nfsd, nd_hash);
		}
	    }
	    mutex_exit(&nfsd_lock);
	}

	/*
	 * Now, do VOP_WRITE()s for any one(s) that need to be done now
	 * and generate the associated reply mbuf list(s).
	 */
loop1:
	getmicrotime(&now);
	cur_usec = (u_quad_t)now.tv_sec * 1000000 + (u_quad_t)now.tv_usec;
	mutex_enter(&nfsd_lock);
	for (nfsd = LIST_FIRST(&slp->ns_tq); nfsd; nfsd = owp) {
		owp = LIST_NEXT(nfsd, nd_tq);
		if (nfsd->nd_time > cur_usec)
		    break;
		if (nfsd->nd_mreq)
		    continue;
		LIST_REMOVE(nfsd, nd_tq);
		LIST_REMOVE(nfsd, nd_hash);
		mutex_exit(&nfsd_lock);

		mrep = nfsd->nd_mrep;
		nfsd->nd_mrep = NULL;
		cred = nfsd->nd_cr;
		v3 = (nfsd->nd_flag & ND_NFSV3);
		forat_ret = aftat_ret = 1;
		error = nfsrv_fhtovp(&nfsd->nd_fh, 1, &vp, cred, slp,
		    nfsd->nd_nam, &rdonly, (nfsd->nd_flag & ND_KERBAUTH),
		    false);
		if (!error) {
		    if (v3)
			forat_ret = VOP_GETATTR(vp, &forat, cred);
		    if (vp->v_type != VREG) {
			if (v3)
			    error = EINVAL;
			else
			    error = (vp->v_type == VDIR) ? EISDIR : EACCES;
		    }
		} else
		    vp = NULL;
		if (!error) {
		    nqsrv_getl(vp, ND_WRITE);
		    error = nfsrv_access(vp, VWRITE, cred, rdonly, lwp, 1);
		}

		if (nfsd->nd_stable == NFSV3WRITE_UNSTABLE)
		    ioflags = IO_NODELOCKED;
		else if (nfsd->nd_stable == NFSV3WRITE_DATASYNC)
		    ioflags = (IO_SYNC | IO_NODELOCKED);
		else
		    ioflags = (IO_METASYNC | IO_SYNC | IO_NODELOCKED);
		uiop->uio_rw = UIO_WRITE;
		uiop->uio_offset = nfsd->nd_off;
		uiop->uio_resid = nfsd->nd_eoff - nfsd->nd_off;
		UIO_SETUP_SYSSPACE(uiop);
		if (uiop->uio_resid > 0) {
		    mp = mrep;
		    i = 0;
		    while (mp) {
			if (mp->m_len > 0)
			    i++;
			mp = mp->m_next;
		    }
		    uiop->uio_iovcnt = i;
		    iov = malloc(i * sizeof (struct iovec), M_TEMP, M_WAITOK);
		    uiop->uio_iov = ivp = iov;
		    mp = mrep;
		    while (mp) {
			if (mp->m_len > 0) {
			    ivp->iov_base = mtod(mp, void *);
			    ivp->iov_len = mp->m_len;
			    ivp++;
			}
			mp = mp->m_next;
		    }
		    if (!error) {
			error = VOP_WRITE(vp, uiop, ioflags, cred);
			nfsstats.srvvop_writes++;
		    }
		    free((void *)iov, M_TEMP);
		}
		m_freem(mrep);
		if (vp) {
		    aftat_ret = VOP_GETATTR(vp, &va, cred);
		    vput(vp);
		}

		/*
		 * Loop around generating replies for all write rpcs that have
		 * now been completed.
		 */
		swp = nfsd;
		do {
		    if (error) {
			nfsm_writereply(NFSX_WCCDATA(v3), v3);
			if (v3) {
			    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
			}
		    } else {
			nfsm_writereply(NFSX_PREOPATTR(v3) +
			    NFSX_POSTOPORFATTR(v3) + 2 * NFSX_UNSIGNED +
			    NFSX_WRITEVERF(v3), v3);
			if (v3) {
			    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
			    nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			    *tl++ = txdr_unsigned(nfsd->nd_len);
			    *tl++ = txdr_unsigned(swp->nd_stable);
			    /*
			     * Actually, there is no need to txdr these fields,
			     * but it may make the values more human readable,
			     * for debugging purposes.
			     */
			    *tl++ = txdr_unsigned(boottime.tv_sec);
			    *tl = txdr_unsigned(boottime.tv_nsec / 1000);
			} else {
			    nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
			    nfsm_srvfillattr(&va, fp);
			}
		    }
		    nfsd->nd_mreq = mreq;
		    if (nfsd->nd_mrep)
			panic("nfsrv_write: nd_mrep not free");

		    /*
		     * Done. Put it at the head of the timer queue so that
		     * the final phase can return the reply.
		     */
		    mutex_enter(&nfsd_lock);
		    if (nfsd != swp) {
			nfsd->nd_time = 0;
			LIST_INSERT_HEAD(&slp->ns_tq, nfsd, nd_tq);
		    }
		    nfsd = LIST_FIRST(&swp->nd_coalesce);
		    if (nfsd) {
			LIST_REMOVE(nfsd, nd_tq);
		    }
		    mutex_exit(&nfsd_lock);
		} while (nfsd);
		swp->nd_time = 0;

		mutex_enter(&nfsd_lock);
		LIST_INSERT_HEAD(&slp->ns_tq, swp, nd_tq);
		mutex_exit(&nfsd_lock);
		goto loop1;
	}
	mutex_exit(&nfsd_lock);
	nfs_timer_start();

	/*
	 * Search for a reply to return.
	 */
	mutex_enter(&nfsd_lock);
	LIST_FOREACH(nfsd, &slp->ns_tq, nd_tq) {
		if (nfsd->nd_mreq) {
		    LIST_REMOVE(nfsd, nd_tq);
		    *mrq = nfsd->nd_mreq;
		    *ndp = nfsd;
		    break;
		}
	}
	mutex_exit(&nfsd_lock);
	return (0);
}

/*
 * Coalesce the write request nfsd into owp. To do this we must:
 * - remove nfsd from the queues
 * - merge nfsd->nd_mrep into owp->nd_mrep
 * - update the nd_eoff and nd_stable for owp
 * - put nfsd on owp's nd_coalesce list
 * NB: Must be called at splsoftclock().
 */
void
nfsrvw_coalesce(struct nfsrv_descript *owp, struct nfsrv_descript *nfsd)
{
        int overlap;
        struct mbuf *mp;
	struct nfsrv_descript *m;

	KASSERT(mutex_owned(&nfsd_lock));

        LIST_REMOVE(nfsd, nd_hash);
        LIST_REMOVE(nfsd, nd_tq);
        if (owp->nd_eoff < nfsd->nd_eoff) {
            overlap = owp->nd_eoff - nfsd->nd_off;
            if (overlap < 0)
                panic("nfsrv_coalesce: bad off");
            if (overlap > 0)
                m_adj(nfsd->nd_mrep, overlap);
            mp = owp->nd_mrep;
            while (mp->m_next)
                mp = mp->m_next;
            mp->m_next = nfsd->nd_mrep;
            owp->nd_eoff = nfsd->nd_eoff;
        } else
            m_freem(nfsd->nd_mrep);
        nfsd->nd_mrep = NULL;
        if (nfsd->nd_stable == NFSV3WRITE_FILESYNC)
            owp->nd_stable = NFSV3WRITE_FILESYNC;
        else if (nfsd->nd_stable == NFSV3WRITE_DATASYNC &&
            owp->nd_stable == NFSV3WRITE_UNSTABLE)
            owp->nd_stable = NFSV3WRITE_DATASYNC;
        LIST_INSERT_HEAD(&owp->nd_coalesce, nfsd, nd_tq);
 	/*
 	 * nfsd might hold coalesce elements! Move them to owp.
 	 * Otherwise, requests may be lost and clients will be stuck.
 	 */
	while ((m = LIST_FIRST(&nfsd->nd_coalesce)) != NULL) {
		LIST_REMOVE(m, nd_tq);
		LIST_INSERT_HEAD(&owp->nd_coalesce, m, nd_tq);
	}
}

/*
 * nfs create service
 * now does a truncate to 0 length via. setattr if it already exists
 */
int
nfsrv_create(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va, dirfor, diraft;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	struct nameidata nd;
	char *cp;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, len, tsize, dirfor_ret = 1, diraft_ret = 1;
	int rdev = 0, abort = 0;
	int v3 = (nfsd->nd_flag & ND_NFSV3), how, exclusive_flag = 0;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp = NULL, *dirp = NULL;
	nfsrvfh_t nsfh;
	u_quad_t frev, tempsize;
	u_char cverf[NFSX_V3CREATEVERF];

	nd.ni_cnd.cn_nameiop = 0;
	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (dirp && v3) {
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		if (dirp)
			vrele(dirp);
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
		return (0);
	}
	abort = 1;
	vattr_null(&va);
	if (v3) {
		va.va_mode = 0;
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		how = fxdr_unsigned(int, *tl);
		switch (how) {
		case NFSV3CREATE_GUARDED:
			if (nd.ni_vp) {
				error = EEXIST;
				break;
			}
		case NFSV3CREATE_UNCHECKED:
			nfsm_srvsattr(&va);
			break;
		case NFSV3CREATE_EXCLUSIVE:
			nfsm_dissect(cp, void *, NFSX_V3CREATEVERF);
			memcpy(cverf, cp, NFSX_V3CREATEVERF);
			exclusive_flag = 1;
			break;
		};
		va.va_type = VREG;
	} else {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		va.va_type = IFTOVT(fxdr_unsigned(u_int32_t, sp->sa_mode));
		if (va.va_type == VNON)
			va.va_type = VREG;
		va.va_mode = nfstov_mode(sp->sa_mode);
		switch (va.va_type) {
		case VREG:
			tsize = fxdr_unsigned(int32_t, sp->sa_size);
			if (tsize != -1)
				va.va_size = (u_quad_t)tsize;
			break;
		case VCHR:
		case VBLK:
		case VFIFO:
			rdev = fxdr_unsigned(int32_t, sp->sa_size);
			break;
		default:
			break;
		};
	}

	/*
	 * Iff doesn't exist, create it
	 * otherwise just truncate to 0 length
	 *   should I set the mode too ??
	 */
	if (nd.ni_vp == NULL) {
		if (va.va_type == VREG || va.va_type == VSOCK) {
			nqsrv_getl(nd.ni_dvp, ND_WRITE);
			error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
			if (!error) {
				vn_lock(nd.ni_vp, LK_EXCLUSIVE | LK_RETRY);
				if (exclusive_flag) {
					exclusive_flag = 0;
					vattr_null(&va);
					/*
					 * XXX
					 * assuming NFSX_V3CREATEVERF
					 * == sizeof(nfstime3)
					 */
					fxdr_nfsv3time(cverf, &va.va_atime);
					error = VOP_SETATTR(nd.ni_vp, &va,
						cred);
				}
			}
		} else if (va.va_type == VCHR || va.va_type == VBLK ||
			va.va_type == VFIFO) {
			if (va.va_type == VCHR && rdev == 0xffffffff)
				va.va_type = VFIFO;
			if (va.va_type != VFIFO &&
			    (error = kauth_authorize_system(cred,
			    KAUTH_SYSTEM_MKNOD, 0, NULL, NULL, NULL))) {
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
				vput(nd.ni_dvp);
				abort = 0;
				nfsm_reply(0);
				if (nd.ni_pathbuf != NULL) {
					pathbuf_destroy(nd.ni_pathbuf);
					nd.ni_pathbuf = NULL;
				}
				return (error);
			} else
				va.va_rdev = (dev_t)rdev;
			nqsrv_getl(nd.ni_dvp, ND_WRITE);
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd,
			    &va);
			if (!error) {
				vn_lock(nd.ni_vp, LK_EXCLUSIVE | LK_RETRY);
			} else {
				nfsm_reply(0);
			}
		} else {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			if (nd.ni_pathbuf != NULL) {
				pathbuf_destroy(nd.ni_pathbuf);
				nd.ni_pathbuf = NULL;
			}
			error = ENXIO;
			abort = 0;
		}
		vp = nd.ni_vp;
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
		vp = nd.ni_vp;
		abort = 0;
		if (!error && va.va_size != -1) {
			error = nfsrv_access(vp, VWRITE, cred,
			    (nd.ni_cnd.cn_flags & RDONLY), lwp, 0);
			if (!error) {
				nqsrv_getl(vp, ND_WRITE);
				tempsize = va.va_size;
				vattr_null(&va);
				va.va_size = tempsize;
				error = VOP_SETATTR(vp, &va, cred);
			}
		}
		if (error)
			vput(vp);
	}
	if (!error) {
		error = nfsrv_composefh(vp, &nsfh, v3);
		if (!error)
			error = VOP_GETATTR(vp, &va, cred);
		vput(vp);
	}
	if (nd.ni_dvp == vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (v3) {
		if (exclusive_flag && !error) {
			/*
			 * XXX assuming NFSX_V3CREATEVERF == sizeof(nfstime3)
			 */
			char oldverf[NFSX_V3CREATEVERF];

			txdr_nfsv3time(&va.va_atime, oldverf);
			if (memcmp(cverf, oldverf, NFSX_V3CREATEVERF))
				error = EEXIST;
		}
		if (dirp) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
			VOP_UNLOCK(dirp);
		}
	}
	if (dirp) {
		vrele(dirp);
		dirp = NULL;
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	abort = 0;
	nfsm_reply(NFSX_SRVFH(&nsfh, v3) + NFSX_FATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(&nsfh);
			nfsm_srvpostop_attr(0, &va);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	} else {
		nfsm_srvfhtom(&nsfh, v3);
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(&va, fp);
	}
	return (0);
nfsmout:
	if (dirp)
		vrele(dirp);
	if (abort) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vput(nd.ni_vp);
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	return (error);
}

/*
 * nfs v3 mknod service
 */
int
nfsrv_mknod(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	u_int32_t *tl;
	struct nameidata nd;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int abort = 0;
	u_int32_t major, minor;
	enum vtype vtyp;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp, *dirp = (struct vnode *)0;
	nfsrvfh_t nsfh;
	u_quad_t frev;

	nd.ni_cnd.cn_nameiop = 0;
	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (dirp)
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
	if (error) {
		nfsm_reply(NFSX_WCCDATA(1));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		if (dirp)
			vrele(dirp);
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
		return (0);
	}
	abort = 1;
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	vtyp = nfsv3tov_type(*tl);
	if (vtyp != VCHR && vtyp != VBLK && vtyp != VSOCK && vtyp != VFIFO) {
		error = NFSERR_BADTYPE;
		goto abort;
	}
	vattr_null(&va);
	va.va_mode = 0;
	nfsm_srvsattr(&va);
	if (vtyp == VCHR || vtyp == VBLK) {
		dev_t rdev;

		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		major = fxdr_unsigned(u_int32_t, *tl++);
		minor = fxdr_unsigned(u_int32_t, *tl);
		rdev = makedev(major, minor);
		if (major(rdev) != major || minor(rdev) != minor) {
			error = EINVAL;
			goto abort;
		}
		va.va_rdev = rdev;
	}

	/*
	 * Iff doesn't exist, create it.
	 */
	if (nd.ni_vp) {
		error = EEXIST;
abort:
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_vp)
			vput(nd.ni_vp);
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
		goto out;
	}
	va.va_type = vtyp;
	if (vtyp == VSOCK) {
		nqsrv_getl(nd.ni_dvp, ND_WRITE);
		error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
	} else {
		if (va.va_type != VFIFO &&
		    (error = kauth_authorize_system(cred,
		    KAUTH_SYSTEM_MKNOD, 0, NULL, NULL, NULL))) {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
			goto out;
		}
		nqsrv_getl(nd.ni_dvp, ND_WRITE);
		error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
		if (error)
			goto out;
	}
	if (!error) {
		vn_lock(nd.ni_vp, LK_EXCLUSIVE | LK_RETRY);
	}
out:
	vp = nd.ni_vp;
	if (!error) {
		error = nfsrv_composefh(vp, &nsfh, true);
		if (!error)
			error = VOP_GETATTR(vp, &va, cred);
		vput(vp);
	}
	if (nd.ni_dvp == nd.ni_vp)
		vrele(nd.ni_dvp);
	else
		vput(nd.ni_dvp);
	if (dirp) {
		vn_lock(dirp, LK_SHARED | LK_RETRY);
		diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
		VOP_UNLOCK(dirp);
		vrele(dirp);
		dirp = NULL;
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	abort = 0;
	nfsm_reply(NFSX_SRVFH(&nsfh, true) + NFSX_POSTOPATTR(1) +
	    NFSX_WCCDATA(1));
	if (!error) {
		nfsm_srvpostop_fh(&nsfh);
		nfsm_srvpostop_attr(0, &va);
	}
	nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	return (0);
nfsmout:
	if (abort) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vput(nd.ni_vp);
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	if (dirp)
		vrele(dirp);
	return (error);
}

/*
 * nfs remove service
 */
int
nfsrv_remove(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct nameidata nd;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp, *dirp;
	struct vattr dirfor, diraft;
	nfsrvfh_t nsfh;
	u_quad_t frev;

#ifndef nolint
	vp = (struct vnode *)0;
#endif
	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error == 0 && dirp && v3) {
		if (nd.ni_dvp == nd.ni_vp)
			vn_lock(dirp, LK_SHARED | LK_RETRY);
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
		if (nd.ni_dvp == nd.ni_vp)
			VOP_UNLOCK(dirp);
	}
	if (!error) {
		vp = nd.ni_vp;
		if (vp->v_type == VDIR) {
			error = EPERM;
			goto out;
		}
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 */
		if (vp->v_vflag & VV_ROOT) {
			error = EBUSY;
		}
out:
		if (!error) {
			nqsrv_getl(nd.ni_dvp, ND_WRITE);
			nqsrv_getl(vp, ND_WRITE);
			error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
		} else {
			VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			if (nd.ni_dvp == vp)
				vrele(nd.ni_dvp);
			else
				vput(nd.ni_dvp);
			vput(vp);
		}
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	if (dirp) {
		if (v3) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
			VOP_UNLOCK(dirp);
		}
		vrele(dirp);
	}
	nfsm_reply(NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		return (0);
	}
	nfsm_srvdone;
}

/*
 * nfs rename service
 */
int
nfsrv_rename(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, fdirfor_ret = 1, fdiraft_ret = 1;
	uint32_t len, len2;
	int tdirfor_ret = 1, tdiraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct nameidata fromnd, tond;
	struct vnode *fvp, *tvp, *tdvp;
	struct vnode *fdirp = NULL, *tdirp = NULL;
	struct mount *localfs = NULL;
	struct vattr fdirfor, fdiraft, tdirfor, tdiraft;
	nfsrvfh_t fnsfh, tnsfh;
	u_quad_t frev;
	uid_t saved_uid;

#ifndef nolint
	fvp = (struct vnode *)0;
#endif
	fromnd.ni_cnd.cn_nameiop = 0;
	tond.ni_cnd.cn_nameiop = 0;
	nfsm_srvmtofh(&fnsfh);
	nfsm_srvnamesiz(len);
	/*
	 * Remember our original uid so that we can reset cr_uid before
	 * the second nfs_namei() call, in case it is remapped.
	 */
	saved_uid = kauth_cred_geteuid(cred);
	fromnd.ni_cnd.cn_cred = cred;
	fromnd.ni_cnd.cn_nameiop = DELETE;
	fromnd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&fromnd, &fnsfh, len, slp, nam, &md,
		&dpos, &fdirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error == 0 && fdirp && v3) {
		if (fromnd.ni_dvp == fromnd.ni_vp)
			vn_lock(fdirp, LK_SHARED | LK_RETRY);
		fdirfor_ret = VOP_GETATTR(fdirp, &fdirfor, cred);
		if (fromnd.ni_dvp == fromnd.ni_vp)
			VOP_UNLOCK(fdirp);
	}
	if (error) {
		fromnd.ni_cnd.cn_nameiop = 0;
		nfsm_reply(2 * NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsm_srvwcc_data(tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
		if (fdirp)
			vrele(fdirp);
		if (fromnd.ni_pathbuf != NULL) {
			pathbuf_destroy(fromnd.ni_pathbuf);
		}
		return (0);
	}
	if (fromnd.ni_dvp != fromnd.ni_vp) {
		VOP_UNLOCK(fromnd.ni_dvp);
	}
	fvp = fromnd.ni_vp;

	localfs = fvp->v_mount;
	error = VFS_RENAMELOCK_ENTER(localfs);
	if (error) {
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}

	/* Copied, regrettably, from vfs_syscalls.c (q.v.) */
	vrele(fvp);
	if ((fromnd.ni_cnd.cn_namelen == 1 && 
	     fromnd.ni_cnd.cn_nameptr[0] == '.') ||
	    (fromnd.ni_cnd.cn_namelen == 2 && 
	     fromnd.ni_cnd.cn_nameptr[0] == '.' &&
	     fromnd.ni_cnd.cn_nameptr[1] == '.')) {
		error = EINVAL;
		VFS_RENAMELOCK_EXIT(localfs);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		goto out1;
	}
	vn_lock(fromnd.ni_dvp, LK_EXCLUSIVE | LK_RETRY);
	error = relookup(fromnd.ni_dvp, &fromnd.ni_vp, &fromnd.ni_cnd, 0);
	if (error) {
		VOP_UNLOCK(fromnd.ni_dvp);
		VFS_RENAMELOCK_EXIT(localfs);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		goto out1;
	}
	VOP_UNLOCK(fromnd.ni_vp);
	if (fromnd.ni_dvp != fromnd.ni_vp)
		VOP_UNLOCK(fromnd.ni_dvp);
	fvp = fromnd.ni_vp;

	nfsm_srvmtofh(&tnsfh);
	if (v3) {
		nfsm_dissect(tl, uint32_t *, NFSX_UNSIGNED);
		len2 = fxdr_unsigned(uint32_t, *tl);
		/* len2 will be checked by nfs_namei */
	}
	else {
		/* NFSv2 */
		nfsm_strsiz(len2, NFS_MAXNAMLEN);
	}
	kauth_cred_seteuid(cred, saved_uid);
	tond.ni_cnd.cn_cred = cred;
	tond.ni_cnd.cn_nameiop = RENAME;
	tond.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | NOCACHE;
	error = nfs_namei(&tond, &tnsfh, len2, slp, nam, &md,
		&dpos, &tdirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (tdirp && v3) {
		tdirfor_ret = VOP_GETATTR(tdirp, &tdirfor, cred);
	}
	if (error) {
		VFS_RENAMELOCK_EXIT(localfs);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			if (v3)
				error = EEXIST;
			else
				error = EISDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			if (v3)
				error = EEXIST;
			else
				error = ENOTDIR;
			goto out;
		}
		if (tvp->v_type == VDIR && tvp->v_mountedhere) {
			if (v3)
				error = EXDEV;
			else
				error = ENOTEMPTY;
			goto out;
		}
	}
	if (fvp->v_type == VDIR && fvp->v_mountedhere) {
		if (v3)
			error = EXDEV;
		else
			error = ENOTEMPTY;
		goto out;
	}
	if (fvp->v_mount != tdvp->v_mount) {
		if (v3)
			error = EXDEV;
		else
			error = ENOTEMPTY;
		goto out;
	}
	if (fvp == tdvp) {
		if (v3)
			error = EINVAL;
		else
			error = ENOTEMPTY;
	}
	/*
	 * If source is the same as the destination (that is the
	 * same vnode with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fvp == tvp && fromnd.ni_dvp == tdvp &&
	    fromnd.ni_cnd.cn_namelen == tond.ni_cnd.cn_namelen &&
	    !memcmp(fromnd.ni_cnd.cn_nameptr, tond.ni_cnd.cn_nameptr,
	      fromnd.ni_cnd.cn_namelen))
		error = -1;
out:
	if (!error) {
		nqsrv_getl(fromnd.ni_dvp, ND_WRITE);
		nqsrv_getl(tdvp, ND_WRITE);
		if (tvp) {
			nqsrv_getl(tvp, ND_WRITE);
		}
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
		VFS_RENAMELOCK_EXIT(localfs);
	} else {
		VOP_ABORTOP(tond.ni_dvp, &tond.ni_cnd);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VFS_RENAMELOCK_EXIT(localfs);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		if (error == -1)
			error = 0;
	}
	if (tond.ni_pathbuf != NULL) {
		pathbuf_destroy(tond.ni_pathbuf);
		tond.ni_pathbuf = NULL;
	}
	tond.ni_cnd.cn_nameiop = 0;
out1:
	if (fdirp) {
		if (v3) {
			vn_lock(fdirp, LK_SHARED | LK_RETRY);
			fdiraft_ret = VOP_GETATTR(fdirp, &fdiraft, cred);
			VOP_UNLOCK(fdirp);
		}
		vrele(fdirp);
		fdirp = NULL;
	}
	if (tdirp) {
		if (v3) {
			vn_lock(tdirp, LK_SHARED | LK_RETRY);
			tdiraft_ret = VOP_GETATTR(tdirp, &tdiraft, cred);
			VOP_UNLOCK(tdirp);
		}
		vrele(tdirp);
		tdirp = NULL;
	}
	pathbuf_destroy(fromnd.ni_pathbuf);
	fromnd.ni_pathbuf = NULL;
	fromnd.ni_cnd.cn_nameiop = 0;
	localfs = NULL;
	nfsm_reply(2 * NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsm_srvwcc_data(tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
	}
	return (0);

nfsmout:
	if (fdirp)
		vrele(fdirp);
#ifdef notdef
	if (tdirp)
		vrele(tdirp);
#endif
	if (tond.ni_cnd.cn_nameiop) {
		if (tond.ni_pathbuf != NULL) {
			pathbuf_destroy(tond.ni_pathbuf);
			tond.ni_pathbuf = NULL;
		}
	}
	if (localfs) {
		VFS_RENAMELOCK_EXIT(localfs);
	}
	if (fromnd.ni_cnd.cn_nameiop) {
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		if (fromnd.ni_pathbuf != NULL) {
			pathbuf_destroy(fromnd.ni_pathbuf);
			fromnd.ni_pathbuf = NULL;
		}
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	return (error);
}

/*
 * nfs link service
 */
int
nfsrv_link(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct nameidata nd;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int getret = 1, v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp, *xp, *dirp = (struct vnode *)0;
	struct vattr dirfor, diraft, at;
	nfsrvfh_t nsfh, dnsfh;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	nfsm_srvmtofh(&dnsfh);
	nfsm_srvnamesiz(len);
	error = nfsrv_fhtovp(&nsfh, false, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
		nfsm_srvpostop_attr(getret, &at);
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		return (0);
	}
	if (vp->v_type == VDIR) {
		error = EPERM;
		goto out1;
	}
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&nd, &dnsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (dirp && v3) {
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
	}
	if (error)
		goto out1;
	xp = nd.ni_vp;
	if (xp != NULL) {
		error = EEXIST;
		goto out;
	}
	xp = nd.ni_dvp;
	if (vp->v_mount != xp->v_mount)
		error = EXDEV;
out:
	if (!error) {
		nqsrv_getl(vp, ND_WRITE);
		nqsrv_getl(xp, ND_WRITE);
		error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
		if (nd.ni_dvp != nd.ni_vp)
			VOP_UNLOCK(nd.ni_dvp);
		vrele(nd.ni_dvp);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
	}
out1:
	if (v3) {
		vn_lock(vp, LK_SHARED | LK_RETRY);
		getret = VOP_GETATTR(vp, &at, cred);
		VOP_UNLOCK(vp);
	}
	if (dirp) {
		if (v3) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
			VOP_UNLOCK(dirp);
		}
		vrele(dirp);
	}
	vrele(vp);
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvpostop_attr(getret, &at);
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		return (0);
	}
	nfsm_srvdone;
}

/*
 * nfs symbolic link service
 */
int
nfsrv_symlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nameidata nd;
	u_int32_t *tl;
	int32_t t1;
	struct nfsv2_sattr *sp;
	char *bpos, *pathcp = NULL, *cp2;
	struct uio io;
	struct iovec iv;
	int error = 0, cache = 0, dirfor_ret = 1, diraft_ret = 1, abort = 0;
	uint32_t len, len2;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq __unused;
	struct vnode *dirp = (struct vnode *)0;
	nfsrvfh_t nsfh;
	u_quad_t frev;

	nd.ni_cnd.cn_nameiop = 0;
	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (dirp && v3) {
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
	}
	if (error)
		goto out;
	abort = 1;
	vattr_null(&va);
	va.va_type = VLNK;
	if (v3) {
		va.va_mode = 0;
		nfsm_srvsattr(&va);
		nfsm_dissect(tl, uint32_t *, NFSX_UNSIGNED);
		len2 = fxdr_unsigned(uint32_t, *tl);
		if (len2 > PATH_MAX) {
			/* XXX should check _PC_NO_TRUNC */
			error = ENAMETOOLONG;
			goto abortop;
		}
	}
	else {
		/* NFSv2 */
		nfsm_strsiz(len2, NFS_MAXPATHLEN);
	}
	pathcp = malloc(len2 + 1, M_TEMP, M_WAITOK);
	iv.iov_base = pathcp;
	iv.iov_len = len2;
	io.uio_resid = len2;
	io.uio_offset = 0;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&io);
	nfsm_mtouio(&io, len2);
	if (!v3) {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		va.va_mode = fxdr_unsigned(u_int16_t, sp->sa_mode);
	}
	*(pathcp + len2) = '\0';
	if (nd.ni_vp) {
		error = EEXIST;
abortop:
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		goto out;
	}
	nqsrv_getl(nd.ni_dvp, ND_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va, pathcp);
	if (!error) {
	    if (v3) {
		vn_lock(nd.ni_vp, LK_SHARED | LK_RETRY);
		error = nfsrv_composefh(nd.ni_vp, &nsfh, v3);
		if (!error)
		    error = VOP_GETATTR(nd.ni_vp, &va, cred);
		vput(nd.ni_vp);
	    } else {
		vrele(nd.ni_vp);
	    }
	}
	vput(nd.ni_dvp);
out:
	if (pathcp)
		free(pathcp, M_TEMP);
	if (dirp) {
		if (v3) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
			VOP_UNLOCK(dirp);
		}
		vrele(dirp);
		dirp = NULL;
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	abort = 0;
	nfsm_reply(NFSX_SRVFH(&nsfh, v3) + NFSX_POSTOPATTR(v3) +
	    NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(&nsfh);
			nfsm_srvpostop_attr(0, &va);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	}
	return (0);
nfsmout:
	if (abort) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
	}
	if (dirp)
		vrele(dirp);
	if (pathcp)
		free(pathcp, M_TEMP);
	return (error);
}

/*
 * nfs mkdir service
 */
int
nfsrv_mkdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nfs_fattr *fp;
	struct nameidata nd;
	char *cp;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int abort = 0;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp, *dirp = (struct vnode *)0;
	nfsrvfh_t nsfh;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (dirp && v3) {
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
	}
	if (error) {
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		if (dirp)
			vrele(dirp);
		return (0);
	}
	abort = 1;
	vattr_null(&va);
	if (v3) {
		va.va_mode = 0;
		nfsm_srvsattr(&va);
	} else {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		va.va_mode = nfstov_mode(*tl++);
	}
	va.va_type = VDIR;
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		error = EEXIST;
		goto out;
	}
	nqsrv_getl(nd.ni_dvp, ND_WRITE);
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &va);
	if (!error) {
		vp = nd.ni_vp;
		vn_lock(vp, LK_SHARED | LK_RETRY);
		error = nfsrv_composefh(vp, &nsfh, v3);
		if (!error)
			error = VOP_GETATTR(vp, &va, cred);
		vput(vp);
	}
	vput(nd.ni_dvp);
out:
	if (dirp) {
		if (v3) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
			VOP_UNLOCK(dirp);
		}
		vrele(dirp);
		dirp = NULL;
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	abort = 0;
	nfsm_reply(NFSX_SRVFH(&nsfh, v3) + NFSX_POSTOPATTR(v3) +
	    NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(&nsfh);
			nfsm_srvpostop_attr(0, &va);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	} else {
		nfsm_srvfhtom(&nsfh, v3);
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(&va, fp);
	}
	return (0);
nfsmout:
	if (abort) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
	}
	if (dirp)
		vrele(dirp);
	return (error);
}

/*
 * nfs rmdir service
 */
int
nfsrv_rmdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp, *dirp = (struct vnode *)0;
	struct vattr dirfor, diraft;
	nfsrvfh_t nsfh;
	struct nameidata nd;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	nfsm_srvnamesiz(len);
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameiop = DELETE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF;
	error = nfs_namei(&nd, &nsfh, len, slp, nam, &md, &dpos,
		&dirp, lwp, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (dirp && v3) {
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, cred);
	}
	if (error) {
		if (nd.ni_pathbuf != NULL) {
			pathbuf_destroy(nd.ni_pathbuf);
			nd.ni_pathbuf = NULL;
		}
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		if (dirp)
			vrele(dirp);
		return (0);
	}
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EINVAL;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_vflag & VV_ROOT)
		error = EBUSY;
out:
	if (!error) {
		nqsrv_getl(nd.ni_dvp, ND_WRITE);
		nqsrv_getl(vp, ND_WRITE);
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	if (nd.ni_pathbuf != NULL) {
		pathbuf_destroy(nd.ni_pathbuf);
		nd.ni_pathbuf = NULL;
	}
	if (dirp) {
		if (v3) {
			vn_lock(dirp, LK_SHARED | LK_RETRY);
			diraft_ret = VOP_GETATTR(dirp, &diraft, cred);
			VOP_UNLOCK(dirp);
		}
		vrele(dirp);
	}
	nfsm_reply(NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		return (0);
	}
	nfsm_srvdone;
}

/*
 * nfs readdir service
 * - mallocs what it thinks is enough to read
 *	count rounded up to a multiple of NFS_SRVDIRBLKSIZ <= NFS_MAXREADDIR
 * - calls VOP_READDIR()
 * - loops around building the reply
 *	if the output generated exceeds count break out of loop
 *	The nfsm_clget macro is used here so that the reply will be packed
 *	tightly in mbuf clusters.
 * - it only knows that it has encountered eof when the VOP_READDIR()
 *	reads nothing
 * - as such one readdir rpc will return eof false although you are there
 *	and then the next will return eof
 * - it trims out records with d_fileno == 0
 *	this doesn't matter for Unix clients, but they might confuse clients
 *	for other os'.
 * - it trims out records with d_type == DT_WHT
 *	these cannot be seen through NFS (unless we extend the protocol)
 * NB: It is tempting to set eof to true if the VOP_READDIR() reads less
 *	than requested, but this may not apply to all filesystems. For
 *	example, client NFS does not { although it is never remote mounted
 *	anyhow }
 *     The alternate call nfsrv_readdirplus() does lookups as well.
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */

#define	NFS_SRVDIRBLKSIZ	1024

struct flrep {
	nfsuint64 fl_off;
	u_int32_t fl_postopok;
	struct nfs_fattr fl_fattr; /* XXX: must be of fattr3 size */
	u_int32_t fl_fhok;
	u_int32_t fl_fhsize;
	/* handle comes here, filled in dynamically */
};

int
nfsrv_readdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	char *bp, *be;
	struct mbuf *mp;
	struct dirent *dp;
	char *cp;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	struct mbuf *mb, *mreq __unused, *mp2;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp;
	struct vattr at;
	nfsrvfh_t nsfh;
	struct uio io;
	struct iovec iv;
	int len, nlen, rem, xfer, tsiz, i, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, cache = 0, ncookies;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	u_quad_t frev, off, toff;
#ifdef NFS3_STRICTVERF
	u_quad_t verf;
#endif
	off_t *cookies = NULL, *cookiep;
	nfsuint64 jar;

	nfsm_srvmtofh(&nsfh);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		toff = fxdr_hyper(tl);
		tl += 2;
#ifdef NFS3_STRICTVERF
		verf = fxdr_hyper(tl);
#endif
		tl += 2;
	} else {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		toff = fxdr_unsigned(u_quad_t, *tl++);
	}
	off = toff;
	cnt = fxdr_unsigned(int, *tl);
	siz = ((cnt + NFS_SRVDIRBLKSIZ - 1) & ~(NFS_SRVDIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (siz > xfer)
		siz = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (!error && vp->v_type != VDIR) {
		error = ENOTDIR;
		vput(vp);
	}
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	nqsrv_getl(vp, ND_READ);
	if (v3) {
		error = getret = VOP_GETATTR(vp, &at, cred);
#ifdef NFS3_STRICTVERF
		/*
		 * XXX This check is too strict for Solaris 2.5 clients.
		 */
		if (!error && toff && verf != at.va_filerev)
			error = NFSERR_BAD_COOKIE;
#endif
	}
	if (!error)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, lwp, 0);
	if (error) {
		vput(vp);
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	VOP_UNLOCK(vp);
	rbuf = malloc(siz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&io);
	eofflag = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	error = VOP_READDIR(vp, &io, cred, &eofflag, &cookies, &ncookies);

	off = (off_t)io.uio_offset;
	if (!cookies && !error)
		error = NFSERR_PERM;
	if (v3) {
		getret = VOP_GETATTR(vp, &at, cred);
		if (!error)
			error = getret;
	}

	VOP_UNLOCK(vp);
	if (error) {
		vrele(vp);
		free((void *)rbuf, M_TEMP);
		if (cookies)
			free((void *)cookies, M_TEMP);
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_COOKIEVERF(v3) +
				2 * NFSX_UNSIGNED);
			if (v3) {
				nfsm_srvpostop_attr(getret, &at);
				nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
				txdr_hyper(at.va_filerev, tl);
				tl += 2;
			} else
				nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = nfs_false;
			*tl = nfs_true;
			free((void *)rbuf, M_TEMP);
			free((void *)cookies, M_TEMP);
			return (0);
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;

	while (cpos < cend && ncookies > 0 &&
		(dp->d_fileno == 0 || dp->d_type == DT_WHT)) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		toff = off;
		siz = fullsiz;
		free(cookies, M_TEMP);
		cookies = NULL;
		goto again;
	}

	len = 3 * NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_COOKIEVERF(v3) + siz);
	if (v3) {
		nfsm_srvpostop_attr(getret, &at);
		nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		txdr_hyper(at.va_filerev, tl);
	}
	mp = mp2 = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;
			len += (4 * NFSX_UNSIGNED + nlen + rem);
			if (v3)
				len += 2 * NFSX_UNSIGNED;
			if (len > cnt) {
				eofflag = 0;
				break;
			}
			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			if (v3) {
				nfsm_clget;
				*tl = txdr_unsigned(dp->d_fileno >> 32);
				bp += NFSX_UNSIGNED;
			}
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;

			/* And loop around copying the name */
			xfer = nlen;
			cp = dp->d_name;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp+xfer) > be)
					tsiz = be-bp;
				else
					tsiz = xfer;
				memcpy(bp, cp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to an int32_t boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';
			nfsm_clget;

			/* Finish off the record */
			txdr_hyper(*cookiep, &jar);
			if (v3) {
				*tl = jar.nfsuquad[0];
				bp += NFSX_UNSIGNED;
				nfsm_clget;
			}
			*tl = jar.nfsuquad[1];
			bp += NFSX_UNSIGNED;
		}
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (mp != mb) {
		if (bp < be)
			mp->m_len = bp - mtod(mp, char *);
	} else
		mp->m_len += bp - bpos;
	free((void *)rbuf, M_TEMP);
	free((void *)cookies, M_TEMP);
	nfsm_srvdone;
}

int
nfsrv_readdirplus(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	char *bp, *be;
	struct mbuf *mp;
	struct dirent *dp;
	char *cp;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	struct mbuf *mb, *mreq __unused, *mp2;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp, *nvp;
	struct flrep fl;
	nfsrvfh_t nsfh;
	struct uio io;
	struct iovec iv;
	struct vattr va, at, *vap = &va;
	struct nfs_fattr *fp;
	int len, nlen, rem, xfer, tsiz, i, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, cache = 0, dirlen, ncookies;
	u_quad_t frev, off, toff;
#ifdef NFS3_STRICTVERF
	u_quad_t verf;
#endif
	off_t *cookies = NULL, *cookiep;

	nfsm_srvmtofh(&nsfh);
	nfsm_dissect(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
	toff = fxdr_hyper(tl);
	tl += 2;
#ifdef NFS3_STRICTVERF
	verf = fxdr_hyper(tl);
#endif
	tl += 2;
	siz = fxdr_unsigned(int, *tl++);
	cnt = fxdr_unsigned(int, *tl);
	off = toff;
	siz = ((siz + NFS_SRVDIRBLKSIZ - 1) & ~(NFS_SRVDIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (siz > xfer)
		siz = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (!error && vp->v_type != VDIR) {
		error = ENOTDIR;
		vput(vp);
	}
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	error = getret = VOP_GETATTR(vp, &at, cred);
#ifdef NFS3_STRICTVERF
	/*
	 * XXX This check is too strict for Solaris 2.5 clients.
	 */
	if (!error && toff && verf != at.va_filerev)
		error = NFSERR_BAD_COOKIE;
#endif
	if (!error) {
		nqsrv_getl(vp, ND_READ);
		error = nfsrv_access(vp, VEXEC, cred, rdonly, lwp, 0);
	}
	if (error) {
		vput(vp);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	VOP_UNLOCK(vp);

	rbuf = malloc(siz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_rw = UIO_READ;
	UIO_SETUP_SYSSPACE(&io);
	eofflag = 0;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	error = VOP_READDIR(vp, &io, cred, &eofflag, &cookies, &ncookies);

	off = (u_quad_t)io.uio_offset;
	getret = VOP_GETATTR(vp, &at, cred);

	VOP_UNLOCK(vp);

	/*
	 * If the VGET operation doesn't work for this filesystem,
	 * we can't support readdirplus. Returning NOTSUPP should
	 * make clients fall back to plain readdir.
	 * There's no need to check for VPTOFH as well, we wouldn't
	 * even be here otherwise.
	 */
	if (!getret) {
		if ((getret = VFS_VGET(vp->v_mount, at.va_fileid, &nvp)))
			getret = (getret == EOPNOTSUPP) ?
				NFSERR_NOTSUPP : NFSERR_IO;
		else
			vput(nvp);
	}

	if (!cookies && !error)
		error = NFSERR_PERM;
	if (!error)
		error = getret;
	if (error) {
		vrele(vp);
		if (cookies)
			free((void *)cookies, M_TEMP);
		free((void *)rbuf, M_TEMP);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF +
				2 * NFSX_UNSIGNED);
			nfsm_srvpostop_attr(getret, &at);
			nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			txdr_hyper(at.va_filerev, tl);
			tl += 2;
			*tl++ = nfs_false;
			*tl = nfs_true;
			free((void *)cookies, M_TEMP);
			free((void *)rbuf, M_TEMP);
			return (0);
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;

	while (cpos < cend && ncookies > 0 &&
		(dp->d_fileno == 0 || dp->d_type == DT_WHT)) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		toff = off;
		siz = fullsiz;
		free(cookies, M_TEMP);
		cookies = NULL;
		goto again;
	}

	dirlen = len = NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF + 2 * NFSX_UNSIGNED;
	nfsm_reply(cnt);
	nfsm_srvpostop_attr(getret, &at);
	nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	txdr_hyper(at.va_filerev, tl);
	mp = mp2 = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT) {
			nfsrvfh_t nnsfh;

			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;

			/*
			 * For readdir_and_lookup get the vnode using
			 * the file number.
			 */
			if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp))
				goto invalid;
			if (nfsrv_composefh(nvp, &nnsfh, true)) {
				vput(nvp);
				goto invalid;
			}
			if (VOP_GETATTR(nvp, vap, cred)) {
				vput(nvp);
				goto invalid;
			}
			vput(nvp);

			/*
			 * If either the dircount or maxcount will be
			 * exceeded, get out now. Both of these lengths
			 * are calculated conservatively, including all
			 * XDR overheads.
			 */
			len += (8 * NFSX_UNSIGNED + nlen + rem + NFSX_V3FH +
				NFSX_V3POSTOPATTR);
			dirlen += (6 * NFSX_UNSIGNED + nlen + rem);
			if (len > cnt || dirlen > fullsiz) {
				eofflag = 0;
				break;
			}

			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			fp = (struct nfs_fattr *)&fl.fl_fattr;
			nfsm_srvfillattr(vap, fp);
			fl.fl_fhsize = txdr_unsigned(NFSX_V3FH);
			fl.fl_fhok = nfs_true;
			fl.fl_postopok = nfs_true;
			txdr_hyper(*cookiep, fl.fl_off.nfsuquad);

			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno >> 32);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;

			/* And loop around copying the name */
			xfer = nlen;
			cp = dp->d_name;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				memcpy(bp, cp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to an int32_t boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';

			/*
			 * Now copy the flrep structure out.
			 */
			xfer = sizeof(struct flrep);
			cp = (void *)&fl;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				memcpy(bp, cp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}

			/*
			 * ... and filehandle.
			 */
			xfer = NFSRVFH_SIZE(&nnsfh);
			cp = NFSRVFH_DATA(&nnsfh);
			while (xfer > 0) {
				nfsm_clget;
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				memcpy(bp, cp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
		}
invalid:
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (mp != mb) {
		if (bp < be)
			mp->m_len = bp - mtod(mp, char *);
	} else
		mp->m_len += bp - bpos;
	free((void *)cookies, M_TEMP);
	free((void *)rbuf, M_TEMP);
	nfsm_srvdone;
}

/*
 * nfs commit service
 */
int
nfsrv_commit(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct vattr bfor, aft;
	struct vnode *vp;
	nfsrvfh_t nsfh;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, for_ret = 1, aft_ret = 1, cache = 0;
	uint32_t cnt;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	u_quad_t frev, off, end;

	nfsm_srvmtofh(&nsfh);
	nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);

	off = fxdr_hyper(tl);
	tl += 2;
	cnt = fxdr_unsigned(uint32_t, *tl);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(for_ret, &bfor, aft_ret, &aft);
		return (0);
	}
	for_ret = VOP_GETATTR(vp, &bfor, cred);
	end = (cnt > 0) ? off + cnt : vp->v_size;
	if (end < off || end > vp->v_size)
		end = vp->v_size;
	if (off < vp->v_size)
		error = VOP_FSYNC(vp, cred, FSYNC_WAIT, off, end);
	/* else error == 0, from nfsrv_fhtovp() */
	aft_ret = VOP_GETATTR(vp, &aft, cred);
	vput(vp);
	nfsm_reply(NFSX_V3WCCDATA + NFSX_V3WRITEVERF);
	nfsm_srvwcc_data(for_ret, &bfor, aft_ret, &aft);
	if (!error) {
		nfsm_build(tl, u_int32_t *, NFSX_V3WRITEVERF);
		*tl++ = txdr_unsigned(boottime.tv_sec);
		*tl = txdr_unsigned(boottime.tv_nsec / 1000);
	} else {
		return (0);
	}
	nfsm_srvdone;
}

/*
 * nfs statfs service
 */
int
nfsrv_statfs(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	struct statvfs *sf = NULL;
	struct nfs_statfs *sfp;
	u_int32_t *tl;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, getret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp;
	struct vattr at;
	nfsrvfh_t nsfh;
	u_quad_t frev, tval;

	nfsm_srvmtofh(&nsfh);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	sf = malloc(sizeof(*sf), M_TEMP, M_WAITOK);
	error = VFS_STATVFS(vp->v_mount, sf);
	getret = VOP_GETATTR(vp, &at, cred);
	vput(vp);
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_STATFS(v3));
	if (v3)
		nfsm_srvpostop_attr(getret, &at);
	if (error) {
		free(sf, M_TEMP);
		return (0);
	}
	nfsm_build(sfp, struct nfs_statfs *, NFSX_STATFS(v3));
	if (v3) {
		tval = (u_quad_t)((quad_t)sf->f_blocks * (quad_t)sf->f_frsize);
		txdr_hyper(tval, &sfp->sf_tbytes);
		tval = (u_quad_t)((quad_t)sf->f_bfree * (quad_t)sf->f_frsize);
		txdr_hyper(tval, &sfp->sf_fbytes);
		tval = (u_quad_t)((quad_t)sf->f_bavail * (quad_t)sf->f_frsize);
		txdr_hyper(tval, &sfp->sf_abytes);
		tval = (u_quad_t)sf->f_files;
		txdr_hyper(tval, &sfp->sf_tfiles);
		tval = (u_quad_t)sf->f_ffree;
		txdr_hyper(tval, &sfp->sf_ffiles);
		txdr_hyper(tval, &sfp->sf_afiles);
		sfp->sf_invarsec = 0;
	} else {
		sfp->sf_tsize = txdr_unsigned(NFS_MAXDGRAMDATA);
		sfp->sf_bsize = txdr_unsigned(sf->f_frsize);
		sfp->sf_blocks = txdr_unsigned(sf->f_blocks);
		sfp->sf_bfree = txdr_unsigned(sf->f_bfree);
		sfp->sf_bavail = txdr_unsigned(sf->f_bavail);
	}
nfsmout:
	if (sf)
	    free(sf, M_TEMP);
	return error;
}

/*
 * nfs fsinfo service
 */
int
nfsrv_fsinfo(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	u_int32_t *tl;
	struct nfsv3_fsinfo *sip;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, getret = 1;
	uint32_t maxdata;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp;
	struct vattr at;
	nfsrvfh_t nsfh;
	u_quad_t frev, maxfsize;
	struct statvfs *sb;

	nfsm_srvmtofh(&nsfh);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}

	/* XXX Try to make a guess on the max file size. */
	sb = malloc(sizeof(*sb), M_TEMP, M_WAITOK);
	VFS_STATVFS(vp->v_mount, sb);
	maxfsize = (u_quad_t)0x80000000 * sb->f_frsize - 1;
	free(sb, M_TEMP);

	getret = VOP_GETATTR(vp, &at, cred);
	vput(vp);
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3FSINFO);
	nfsm_srvpostop_attr(getret, &at);
	nfsm_build(sip, struct nfsv3_fsinfo *, NFSX_V3FSINFO);

	/*
	 * XXX
	 * There should be file system VFS OP(s) to get this information.
	 * For now, assume ufs.
	 */
	if (slp->ns_so->so_type == SOCK_DGRAM)
		maxdata = NFS_MAXDGRAMDATA;
	else
		maxdata = NFS_MAXDATA;
	sip->fs_rtmax = txdr_unsigned(maxdata);
	sip->fs_rtpref = txdr_unsigned(maxdata);
	sip->fs_rtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_wtmax = txdr_unsigned(maxdata);
	sip->fs_wtpref = txdr_unsigned(maxdata);
	sip->fs_wtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_dtpref = txdr_unsigned(maxdata);
	txdr_hyper(maxfsize, &sip->fs_maxfilesize);
	sip->fs_timedelta.nfsv3_sec = 0;
	sip->fs_timedelta.nfsv3_nsec = txdr_unsigned(1);
	sip->fs_properties = txdr_unsigned(NFSV3FSINFO_LINK |
		NFSV3FSINFO_SYMLINK | NFSV3FSINFO_HOMOGENEOUS |
		NFSV3FSINFO_CANSETTIME);
	nfsm_srvdone;
}

/*
 * nfs pathconf service
 */
int
nfsrv_pathconf(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp, struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct mbuf *nam = nfsd->nd_nam;
	char *dpos = nfsd->nd_dpos;
	kauth_cred_t cred = nfsd->nd_cr;
	u_int32_t *tl;
	struct nfsv3_pathconf *pc;
	int32_t t1;
	char *bpos;
	int error = 0, rdonly, cache = 0, getret = 1;
	register_t linkmax, namemax, chownres, notrunc;
	char *cp2;
	struct mbuf *mb, *mreq __unused;
	struct vnode *vp;
	struct vattr at;
	nfsrvfh_t nsfh;
	u_quad_t frev;

	nfsm_srvmtofh(&nsfh);
	error = nfsrv_fhtovp(&nsfh, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), false);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		return (0);
	}
	error = VOP_PATHCONF(vp, _PC_LINK_MAX, &linkmax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NAME_MAX, &namemax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_CHOWN_RESTRICTED, &chownres);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NO_TRUNC, &notrunc);
	getret = VOP_GETATTR(vp, &at, cred);
	vput(vp);
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3PATHCONF);
	nfsm_srvpostop_attr(getret, &at);
	if (error)
		return (0);
	nfsm_build(pc, struct nfsv3_pathconf *, NFSX_V3PATHCONF);

	pc->pc_linkmax = txdr_unsigned(linkmax);
	pc->pc_namemax = txdr_unsigned(namemax);
	pc->pc_notrunc = txdr_unsigned(notrunc);
	pc->pc_chownrestricted = txdr_unsigned(chownres);

	/*
	 * These should probably be supported by VOP_PATHCONF(), but
	 * until msdosfs is exportable (why would you want to?), the
	 * Unix defaults should be ok.
	 */
	pc->pc_caseinsensitive = nfs_false;
	pc->pc_casepreserving = nfs_true;
	nfsm_srvdone;
}

/*
 * Null operation, used by clients to ping server
 */
/* ARGSUSED */
int
nfsrv_null(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep;
	char *bpos;
	int error = NFSERR_RETVOID, cache = 0;
	struct mbuf *mb, *mreq __unused;
	u_quad_t frev;

	nfsm_reply(0);
nfsmout:
	return (0);
}

/*
 * No operation, used for obsolete procedures
 */
/* ARGSUSED */
int
nfsrv_noop(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
    struct lwp *lwp, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep;
	char *bpos;
	int error, cache = 0;
	struct mbuf *mb, *mreq __unused;
	u_quad_t frev;

	if (nfsd->nd_repstat)
		error = nfsd->nd_repstat;
	else
		error = EPROCUNAVAIL;
	nfsm_reply(0);
nfsmout:
	return (0);
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client. You cannot just use
 * vn_writechk() and VOP_ACCESS() for two reasons.
 * 1 - You must check for exported rdonly as well as MNT_RDONLY for the write case
 * 2 - The owner is to be given access irrespective of mode bits for some
 *     operations, so that processes that chmod after opening a file don't
 *     break. I don't like this because it opens a security hole, but since
 *     the nfs server opens a security hole the size of a barn door anyhow,
 *     what the heck.
 *
 * The exception to rule 2 is EPERM. If a file is IMMUTABLE, VOP_ACCESS()
 * will return EPERM instead of EACCES. EPERM is always an error.
 */
int
nfsrv_access(struct vnode *vp, int flags, kauth_cred_t cred, int rdonly, struct lwp *lwp, int override)
{
	struct vattr vattr;
	int error;
	if (flags & VWRITE) {
		/* Just vn_writechk() changed to check rdonly */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			switch (vp->v_type) {
			case VREG:
			case VDIR:
			case VLNK:
				return (EROFS);
			default:
				break;
			}
		}

		/*
		 * If the vnode is in use as a process's text,
		 * we can't allow writing.
		 */
		if (vp->v_iflag & VI_TEXT)
			return (ETXTBSY);
	}
	error = VOP_GETATTR(vp, &vattr, cred);
	if (error)
		return (error);
	error = VOP_ACCESS(vp, flags, cred);
	/*
	 * Allow certain operations for the owner (reads and writes
	 * on files that are already open).
	 */
	if (override && error == EACCES && kauth_cred_geteuid(cred) == vattr.va_uid)
		error = 0;
	return error;
}
