/*	$NetBSD: nfs_nfsdserv.c,v 1.2 2014/03/25 16:30:28 christos Exp $	*/
/*-
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
 * 4. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
/* __FBSDID("FreeBSD: head/sys/fs/nfsserver/nfs_nfsdserv.c 245613 2013-01-18 19:42:08Z delphij "); */
__RCSID("$NetBSD: nfs_nfsdserv.c,v 1.2 2014/03/25 16:30:28 christos Exp $");

/*
 * nfs version 2, 3 and 4 server calls to vnode ops
 * - these routines generally have 3 phases
 *   1 - break down and validate rpc request in mbuf list
 *   2 - do the vnode ops for the request, usually by calling a nfsvno_XXX()
 *       function in nfsd_port.c
 *   3 - build the rpc reply in an mbuf list
 * For nfsv4, these functions are called for each Op within the Compound RPC.
 */

#ifndef APPLEKEXT
#include <fs/nfs/nfsport.h>

/* Global vars */
extern u_int32_t newnfs_false, newnfs_true;
extern enum vtype nv34tov_type[8];
extern struct timeval nfsboottime;
extern int nfs_rootfhset;
extern int nfsrv_enable_crossmntpt;
#endif	/* !APPLEKEXT */

static int	nfs_async = 0;
SYSCTL_DECL(_vfs_nfsd);
SYSCTL_INT(_vfs_nfsd, OID_AUTO, async, CTLFLAG_RW, &nfs_async, 0,
    "Tell client that writes were synced even though they were not");

/*
 * This list defines the GSS mechanisms supported.
 * (Don't ask me how you get these strings from the RFC stuff like
 *  iso(1), org(3)... but someone did it, so I don't need to know.)
 */
static struct nfsgss_mechlist nfsgss_mechlist[] = {
	{ 9, "\052\206\110\206\367\022\001\002\002", 11 },
	{ 0, "", 0 },
};

/* local functions */
static void nfsrvd_symlinksub(struct nfsrv_descript *nd, struct nameidata *ndp,
    struct nfsvattr *nvap, fhandle_t *fhp, vnode_t *vpp,
    vnode_t dirp, struct nfsvattr *dirforp, struct nfsvattr *diraftp,
    int *diraft_retp, nfsattrbit_t *attrbitp,
    NFSACL_T *aclp, NFSPROC_T *p, struct nfsexstuff *exp, char *pathcp,
    int pathlen);
static void nfsrvd_mkdirsub(struct nfsrv_descript *nd, struct nameidata *ndp,
    struct nfsvattr *nvap, fhandle_t *fhp, vnode_t *vpp,
    vnode_t dirp, struct nfsvattr *dirforp, struct nfsvattr *diraftp,
    int *diraft_retp, nfsattrbit_t *attrbitp, NFSACL_T *aclp,
    NFSPROC_T *p, struct nfsexstuff *exp);

/*
 * nfs access service (not a part of NFS V2)
 */
APPLESTATIC int
nfsrvd_access(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int getret, error = 0;
	struct nfsvattr nva;
	u_int32_t testmode, nfsmode, supported = 0;
	accmode_t deletebit;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, 1, &nva);
		goto out;
	}
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	nfsmode = fxdr_unsigned(u_int32_t, *tl);
	if ((nd->nd_flag & ND_NFSV4) &&
	    (nfsmode & ~(NFSACCESS_READ | NFSACCESS_LOOKUP |
	     NFSACCESS_MODIFY | NFSACCESS_EXTEND | NFSACCESS_DELETE |
	     NFSACCESS_EXECUTE))) {
		nd->nd_repstat = NFSERR_INVAL;
		vput(vp);
		goto out;
	}
	if (nfsmode & NFSACCESS_READ) {
		supported |= NFSACCESS_READ;
		if (nfsvno_accchk(vp, VREAD, nd->nd_cred, exp, p,
		    NFSACCCHK_NOOVERRIDE, NFSACCCHK_VPISLOCKED, &supported))
			nfsmode &= ~NFSACCESS_READ;
	}
	if (nfsmode & NFSACCESS_MODIFY) {
		supported |= NFSACCESS_MODIFY;
		if (nfsvno_accchk(vp, VWRITE, nd->nd_cred, exp, p,
		    NFSACCCHK_NOOVERRIDE, NFSACCCHK_VPISLOCKED, &supported))
			nfsmode &= ~NFSACCESS_MODIFY;
	}
	if (nfsmode & NFSACCESS_EXTEND) {
		supported |= NFSACCESS_EXTEND;
		if (nfsvno_accchk(vp, VWRITE | VAPPEND, nd->nd_cred, exp, p,
		    NFSACCCHK_NOOVERRIDE, NFSACCCHK_VPISLOCKED, &supported))
			nfsmode &= ~NFSACCESS_EXTEND;
	}
	if (nfsmode & NFSACCESS_DELETE) {
		supported |= NFSACCESS_DELETE;
		if (vp->v_type == VDIR)
			deletebit = VDELETE_CHILD;
		else
			deletebit = VDELETE;
		if (nfsvno_accchk(vp, deletebit, nd->nd_cred, exp, p,
		    NFSACCCHK_NOOVERRIDE, NFSACCCHK_VPISLOCKED, &supported))
			nfsmode &= ~NFSACCESS_DELETE;
	}
	if (vnode_vtype(vp) == VDIR)
		testmode = NFSACCESS_LOOKUP;
	else
		testmode = NFSACCESS_EXECUTE;
	if (nfsmode & testmode) {
		supported |= (nfsmode & testmode);
		if (nfsvno_accchk(vp, VEXEC, nd->nd_cred, exp, p,
		    NFSACCCHK_NOOVERRIDE, NFSACCCHK_VPISLOCKED, &supported))
			nfsmode &= ~testmode;
	}
	nfsmode &= supported;
	if (nd->nd_flag & ND_NFSV3) {
		getret = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
		nfsrv_postopattr(nd, getret, &nva);
	}
	vput(vp);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(supported);
	} else
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(nfsmode);

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs getattr service
 */
APPLESTATIC int
nfsrvd_getattr(struct nfsrv_descript *nd, int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	struct nfsvattr nva;
	fhandle_t fh;
	int at_root = 0, error = 0, supports_nfsv4acls;
	struct nfsreferral *refp;
	nfsattrbit_t attrbits, tmpbits;
	struct mount *mp;
	struct vnode *tvp = NULL;
	struct vattr va;
	uint64_t mounted_on_fileno = 0;
	accmode_t accmode;

	if (nd->nd_repstat)
		goto out;
	if (nd->nd_flag & ND_NFSV4) {
		error = nfsrv_getattrbits(nd, &attrbits, NULL, NULL);
		if (error) {
			vput(vp);
			goto out;
		}

		/*
		 * Check for a referral.
		 */
		refp = nfsv4root_getreferral(vp, NULL, 0);
		if (refp != NULL) {
			(void) nfsrv_putreferralattr(nd, &attrbits, refp, 1,
			    &nd->nd_repstat);
			vput(vp);
			goto out;
		}
		if (nd->nd_repstat == 0) {
			accmode = 0;
			NFSSET_ATTRBIT(&tmpbits, &attrbits);
			if (NFSISSET_ATTRBIT(&tmpbits, NFSATTRBIT_ACL)) {
				NFSCLRBIT_ATTRBIT(&tmpbits, NFSATTRBIT_ACL);
				accmode |= VREAD_ACL;
			}
			if (NFSNONZERO_ATTRBIT(&tmpbits))
				accmode |= VREAD_ATTRIBUTES;
			if (accmode != 0)
				nd->nd_repstat = nfsvno_accchk(vp, accmode,
				    nd->nd_cred, exp, p, NFSACCCHK_NOOVERRIDE,
				    NFSACCCHK_VPISLOCKED, NULL);
		}
	}
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV4) {
			if (NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_FILEHANDLE))
				nd->nd_repstat = nfsvno_getfh(vp, &fh, p);
			if (!nd->nd_repstat)
				nd->nd_repstat = nfsrv_checkgetattr(nd, vp,
				    &nva, &attrbits, nd->nd_cred, p);
			if (nd->nd_repstat == 0) {
				supports_nfsv4acls = nfs_supportsnfsv4acls(vp);
				mp = vp->v_mount;
				if (nfsrv_enable_crossmntpt != 0 &&
				    vp->v_type == VDIR &&
				    (vp->v_vflag & VV_ROOT) != 0 &&
				    vp != rootvnode) {
					tvp = mp->mnt_vnodecovered;
					VREF(tvp);
					at_root = 1;
				} else
					at_root = 0;
				vfs_ref(mp);
				NFSVOPUNLOCK(vp, 0);
				if (at_root != 0) {
					if ((nd->nd_repstat =
					     NFSVOPLOCK(tvp, LK_SHARED)) == 0) {
						nd->nd_repstat = VOP_GETATTR(
						    tvp, &va, nd->nd_cred);
						vput(tvp);
					} else
						vrele(tvp);
					if (nd->nd_repstat == 0)
						mounted_on_fileno = (uint64_t)
						    va.va_fileid;
					else
						at_root = 0;
				}
				if (nd->nd_repstat == 0)
					nd->nd_repstat = vfs_busy(mp, 0);
				vfs_rel(mp);
				if (nd->nd_repstat == 0) {
					(void)nfsvno_fillattr(nd, mp, vp, &nva,
					    &fh, 0, &attrbits, nd->nd_cred, p,
					    isdgram, 1, supports_nfsv4acls,
					    at_root, mounted_on_fileno);
					vfs_unbusy(mp);
				}
				vrele(vp);
			} else
				vput(vp);
		} else {
			nfsrv_fillattr(nd, &nva);
			vput(vp);
		}
	} else {
		vput(vp);
	}

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs setattr service
 */
APPLESTATIC int
nfsrvd_setattr(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	struct nfsvattr nva, nva2;
	u_int32_t *tl;
	int preat_ret = 1, postat_ret = 1, gcheck = 0, error = 0;
	struct timespec guard = { 0, 0 };
	nfsattrbit_t attrbits, retbits;
	nfsv4stateid_t stateid;
	NFSACL_T *aclp = NULL;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, preat_ret, &nva2, postat_ret, &nva);
		goto out;
	}
#ifdef NFS4_ACL_EXTATTR_NAME
	aclp = acl_alloc(M_WAITOK);
	aclp->acl_cnt = 0;
#endif
	NFSVNO_ATTRINIT(&nva);
	NFSZERO_ATTRBIT(&retbits);
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
		stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
		NFSBCOPY((caddr_t)tl,(caddr_t)stateid.other,NFSX_STATEIDOTHER);
	}
	error = nfsrv_sattr(nd, &nva, &attrbits, aclp, p);
	if (error)
		goto nfsmout;
	preat_ret = nfsvno_getattr(vp, &nva2, nd->nd_cred, p, 1);
	if (!nd->nd_repstat)
		nd->nd_repstat = preat_ret;
	if (nd->nd_flag & ND_NFSV3) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		gcheck = fxdr_unsigned(int, *tl);
		if (gcheck) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &guard);
		}
		if (!nd->nd_repstat && gcheck &&
		    (nva2.na_ctime.tv_sec != guard.tv_sec ||
		     nva2.na_ctime.tv_nsec != guard.tv_nsec))
			nd->nd_repstat = NFSERR_NOT_SYNC;
		if (nd->nd_repstat) {
			vput(vp);
#ifdef NFS4_ACL_EXTATTR_NAME
			acl_free(aclp);
#endif
			nfsrv_wcc(nd, preat_ret, &nva2, postat_ret, &nva);
			goto out;
		}
	} else if (!nd->nd_repstat && (nd->nd_flag & ND_NFSV4))
		nd->nd_repstat = nfsrv_checkuidgid(nd, &nva);

	/*
	 * Now that we have all the fields, lets do it.
	 * If the size is being changed write access is required, otherwise
	 * just check for a read only file system.
	 */
	if (!nd->nd_repstat) {
		if (NFSVNO_NOTSETSIZE(&nva)) {
			if (NFSVNO_EXRDONLY(exp) ||
			    (vfs_flags(vnode_mount(vp)) & MNT_RDONLY))
				nd->nd_repstat = EROFS;
		} else {
			if (vnode_vtype(vp) != VREG)
				nd->nd_repstat = EINVAL;
			else if (nva2.na_uid != nd->nd_cred->cr_uid ||
			    NFSVNO_EXSTRICTACCESS(exp))
				nd->nd_repstat = nfsvno_accchk(vp,
				    VWRITE, nd->nd_cred, exp, p,
				    NFSACCCHK_NOOVERRIDE,
				    NFSACCCHK_VPISLOCKED, NULL);
		}
	}
	if (!nd->nd_repstat && (nd->nd_flag & ND_NFSV4))
		nd->nd_repstat = nfsrv_checksetattr(vp, nd, &stateid,
		    &nva, &attrbits, exp, p);

	if (!nd->nd_repstat && (nd->nd_flag & ND_NFSV4)) {
	    /*
	     * For V4, try setting the attrbutes in sets, so that the
	     * reply bitmap will be correct for an error case.
	     */
	    if (NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_OWNER) ||
		NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_OWNERGROUP)) {
		NFSVNO_ATTRINIT(&nva2);
		NFSVNO_SETATTRVAL(&nva2, uid, nva.na_uid);
		NFSVNO_SETATTRVAL(&nva2, gid, nva.na_gid);
		nd->nd_repstat = nfsvno_setattr(vp, &nva2, nd->nd_cred, p,
		    exp);
		if (!nd->nd_repstat) {
		    if (NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_OWNER))
			NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_OWNER);
		    if (NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_OWNERGROUP))
			NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_OWNERGROUP);
		}
	    }
	    if (!nd->nd_repstat &&
		NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_SIZE)) {
		NFSVNO_ATTRINIT(&nva2);
		NFSVNO_SETATTRVAL(&nva2, size, nva.na_size);
		nd->nd_repstat = nfsvno_setattr(vp, &nva2, nd->nd_cred, p,
		    exp);
		if (!nd->nd_repstat)
		    NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_SIZE);
	    }
	    if (!nd->nd_repstat &&
		(NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_TIMEACCESSSET) ||
		 NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_TIMEMODIFYSET))) {
		NFSVNO_ATTRINIT(&nva2);
		NFSVNO_SETATTRVAL(&nva2, atime, nva.na_atime);
		NFSVNO_SETATTRVAL(&nva2, mtime, nva.na_mtime);
		if (nva.na_vaflags & VA_UTIMES_NULL) {
			nva2.na_vaflags |= VA_UTIMES_NULL;
			NFSVNO_SETACTIVE(&nva2, vaflags);
		}
		nd->nd_repstat = nfsvno_setattr(vp, &nva2, nd->nd_cred, p,
		    exp);
		if (!nd->nd_repstat) {
		    if (NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_TIMEACCESSSET))
			NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_TIMEACCESSSET);
		    if (NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_TIMEMODIFYSET))
			NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_TIMEMODIFYSET);
		}
	    }
	    if (!nd->nd_repstat &&
		NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_MODE)) {
		NFSVNO_ATTRINIT(&nva2);
		NFSVNO_SETATTRVAL(&nva2, mode, nva.na_mode);
		nd->nd_repstat = nfsvno_setattr(vp, &nva2, nd->nd_cred, p,
		    exp);
		if (!nd->nd_repstat)
		    NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_MODE);
	    }

#ifdef NFS4_ACL_EXTATTR_NAME
	    if (!nd->nd_repstat && aclp->acl_cnt > 0 &&
		NFSISSET_ATTRBIT(&attrbits, NFSATTRBIT_ACL)) {
		nd->nd_repstat = nfsrv_setacl(vp, aclp, nd->nd_cred, p);
		if (!nd->nd_repstat) 
		    NFSSETBIT_ATTRBIT(&retbits, NFSATTRBIT_ACL);
	    }
#endif
	} else if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_setattr(vp, &nva, nd->nd_cred, p,
		    exp);
	}
	if (nd->nd_flag & (ND_NFSV2 | ND_NFSV3)) {
		postat_ret = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
		if (!nd->nd_repstat)
			nd->nd_repstat = postat_ret;
	}
	vput(vp);
#ifdef NFS4_ACL_EXTATTR_NAME
	acl_free(aclp);
#endif
	if (nd->nd_flag & ND_NFSV3)
		nfsrv_wcc(nd, preat_ret, &nva2, postat_ret, &nva);
	else if (nd->nd_flag & ND_NFSV4)
		(void) nfsrv_putattrbit(nd, &retbits);
	else if (!nd->nd_repstat)
		nfsrv_fillattr(nd, &nva);

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
#ifdef NFS4_ACL_EXTATTR_NAME
	acl_free(aclp);
#endif
	if (nd->nd_flag & ND_NFSV4) {
		/*
		 * For all nd_repstat, the V4 reply includes a bitmap,
		 * even NFSERR_BADXDR, which is what this will end up
		 * returning.
		 */
		(void) nfsrv_putattrbit(nd, &retbits);
	}
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs lookup rpc
 * (Also performs lookup parent for v4)
 */
APPLESTATIC int
nfsrvd_lookup(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, vnode_t *vpp, fhandle_t *fhp, NFSPROC_T *p,
    struct nfsexstuff *exp)
{
	struct nameidata named;
	vnode_t vp, dirp = NULL;
	int error = 0, dattr_ret = 1;
	struct nfsvattr nva, dattr;
	char *bufp;
	u_long *hashp;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, dattr_ret, &dattr);
		goto out;
	}

	/*
	 * For some reason, if dp is a symlink, the error
	 * returned is supposed to be NFSERR_SYMLINK and not NFSERR_NOTDIR.
	 */
	if (dp->v_type == VLNK && (nd->nd_flag & ND_NFSV4)) {
		nd->nd_repstat = NFSERR_SYMLINK;
		vrele(dp);
		goto out;
	}

	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, LOOKUP,
	    LOCKLEAF | SAVESTART);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (error) {
		vrele(dp);
		nfsvno_relpathbuf(&named);
		goto out;
	}
	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_namei(nd, &named, dp, 0, exp, p, &dirp);
	} else {
		vrele(dp);
		nfsvno_relpathbuf(&named);
	}
	if (nd->nd_repstat) {
		if (dirp) {
			if (nd->nd_flag & ND_NFSV3)
				dattr_ret = nfsvno_getattr(dirp, &dattr,
				    nd->nd_cred, p, 0);
			vrele(dirp);
		}
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, dattr_ret, &dattr);
		goto out;
	}
	if (named.ni_startdir)
		vrele(named.ni_startdir);
	nfsvno_relpathbuf(&named);
	vp = named.ni_vp;
	if ((nd->nd_flag & ND_NFSV4) != 0 && !NFSVNO_EXPORTED(exp) &&
	    vp->v_type != VDIR && vp->v_type != VLNK)
		/*
		 * Only allow lookup of VDIR and VLNK for traversal of
		 * non-exported volumes during NFSv4 mounting.
		 */
		nd->nd_repstat = ENOENT;
	if (nd->nd_repstat == 0)
		nd->nd_repstat = nfsvno_getfh(vp, fhp, p);
	if (!(nd->nd_flag & ND_NFSV4) && !nd->nd_repstat)
		nd->nd_repstat = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
	if (vpp != NULL && nd->nd_repstat == 0)
		*vpp = vp;
	else
		vput(vp);
	if (dirp) {
		if (nd->nd_flag & ND_NFSV3)
			dattr_ret = nfsvno_getattr(dirp, &dattr, nd->nd_cred,
			    p, 0);
		vrele(dirp);
	}
	if (nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, dattr_ret, &dattr);
		goto out;
	}
	if (nd->nd_flag & ND_NFSV2) {
		(void) nfsm_fhtom(nd, (u_int8_t *)fhp, 0, 0);
		nfsrv_fillattr(nd, &nva);
	} else if (nd->nd_flag & ND_NFSV3) {
		(void) nfsm_fhtom(nd, (u_int8_t *)fhp, 0, 0);
		nfsrv_postopattr(nd, 0, &nva);
		nfsrv_postopattr(nd, dattr_ret, &dattr);
	}

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs readlink service
 */
APPLESTATIC int
nfsrvd_readlink(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	mbuf_t mp = NULL, mpend = NULL;
	int getret = 1, len;
	struct nfsvattr nva;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &nva);
		goto out;
	}
	if (vnode_vtype(vp) != VLNK) {
		if (nd->nd_flag & ND_NFSV2)
			nd->nd_repstat = ENXIO;
		else
			nd->nd_repstat = EINVAL;
	}
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_readlink(vp, nd->nd_cred, p,
		    &mp, &mpend, &len);
	if (nd->nd_flag & ND_NFSV3)
		getret = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
	vput(vp);
	if (nd->nd_flag & ND_NFSV3)
		nfsrv_postopattr(nd, getret, &nva);
	if (nd->nd_repstat)
		goto out;
	NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	mbuf_setnext(nd->nd_mb, mp);
	nd->nd_mb = mpend;
	nd->nd_bpos = NFSMTOD(mpend, caddr_t) + mbuf_len(mpend);

out:
	NFSEXITCODE2(0, nd);
	return (0);
}

/*
 * nfs read service
 */
APPLESTATIC int
nfsrvd_read(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int error = 0, cnt, getret = 1, reqlen, eof = 0;
	mbuf_t m2, m3;
	struct nfsvattr nva;
	off_t off = 0x0;
	struct nfsstate st, *stp = &st;
	struct nfslock lo, *lop = &lo;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &nva);
		goto out;
	}
	if (nd->nd_flag & ND_NFSV2) {
		NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *tl++);
		reqlen = fxdr_unsigned(int, *tl);
	} else if (nd->nd_flag & ND_NFSV3) {
		NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 2;
		reqlen = fxdr_unsigned(int, *tl);
	} else {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID + 3*NFSX_UNSIGNED);
		reqlen = fxdr_unsigned(int, *(tl + 6));
	}
	if (reqlen > NFS_SRVMAXDATA(nd)) {
		reqlen = NFS_SRVMAXDATA(nd);
	} else if (reqlen < 0) {
		error = EBADRPC;
		goto nfsmout;
	}
	if (nd->nd_flag & ND_NFSV4) {
		stp->ls_flags = (NFSLCK_CHECK | NFSLCK_READACCESS);
		lop->lo_flags = NFSLCK_READ;
		stp->ls_ownerlen = 0;
		stp->ls_op = NULL;
		stp->ls_uid = nd->nd_cred->cr_uid;
		stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
		clientid.lval[0] = stp->ls_stateid.other[0] = *tl++;
		clientid.lval[1] = stp->ls_stateid.other[1] = *tl++;
		if (nd->nd_flag & ND_IMPLIEDCLID) {
			if (nd->nd_clientid.qval != clientid.qval)
				printf("EEK! multiple clids\n");
		} else {
			nd->nd_flag |= ND_IMPLIEDCLID;
			nd->nd_clientid.qval = clientid.qval;
		}
		stp->ls_stateid.other[2] = *tl++;
		off = fxdr_hyper(tl);
		lop->lo_first = off;
		tl += 2;
		lop->lo_end = off + reqlen;
		/*
		 * Paranoia, just in case it wraps around.
		 */
		if (lop->lo_end < off)
			lop->lo_end = NFS64BITSSET;
	}
	if (vnode_vtype(vp) != VREG) {
		if (nd->nd_flag & ND_NFSV3)
			nd->nd_repstat = EINVAL;
		else
			nd->nd_repstat = (vnode_vtype(vp) == VDIR) ? EISDIR :
			    EINVAL;
	}
	getret = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
	if (!nd->nd_repstat)
		nd->nd_repstat = getret;
	if (!nd->nd_repstat &&
	    (nva.na_uid != nd->nd_cred->cr_uid ||
	     NFSVNO_EXSTRICTACCESS(exp))) {
		nd->nd_repstat = nfsvno_accchk(vp, VREAD,
		    nd->nd_cred, exp, p,
		    NFSACCCHK_ALLOWOWNER, NFSACCCHK_VPISLOCKED, NULL);
		if (nd->nd_repstat)
			nd->nd_repstat = nfsvno_accchk(vp, VEXEC,
			    nd->nd_cred, exp, p, NFSACCCHK_ALLOWOWNER,
			    NFSACCCHK_VPISLOCKED, NULL);
	}
	if ((nd->nd_flag & ND_NFSV4) && !nd->nd_repstat)
		nd->nd_repstat = nfsrv_lockctrl(vp, &stp, &lop, NULL, clientid,
		    &stateid, exp, nd, p);
	if (nd->nd_repstat) {
		vput(vp);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_postopattr(nd, getret, &nva);
		goto out;
	}
	if (off >= nva.na_size) {
		cnt = 0;
		eof = 1;
	} else if (reqlen == 0)
		cnt = 0;
	else if ((off + reqlen) >= nva.na_size) {
		cnt = nva.na_size - off;
		eof = 1;
	} else
		cnt = reqlen;
	m3 = NULL;
	if (cnt > 0) {
		nd->nd_repstat = nfsvno_read(vp, off, cnt, nd->nd_cred, p,
		    &m3, &m2);
		if (!(nd->nd_flag & ND_NFSV4)) {
			getret = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
			if (!nd->nd_repstat)
				nd->nd_repstat = getret;
		}
		if (nd->nd_repstat) {
			vput(vp);
			if (m3)
				mbuf_freem(m3);
			if (nd->nd_flag & ND_NFSV3)
				nfsrv_postopattr(nd, getret, &nva);
			goto out;
		}
	}
	vput(vp);
	if (nd->nd_flag & ND_NFSV2) {
		nfsrv_fillattr(nd, &nva);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
	} else {
		if (nd->nd_flag & ND_NFSV3) {
			nfsrv_postopattr(nd, getret, &nva);
			NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(cnt);
		} else
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		if (eof)
			*tl++ = newnfs_true;
		else
			*tl++ = newnfs_false;
	}
	*tl = txdr_unsigned(cnt);
	if (m3) {
		mbuf_setnext(nd->nd_mb, m3);
		nd->nd_mb = m2;
		nd->nd_bpos = NFSMTOD(m2, caddr_t) + mbuf_len(m2);
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs write service
 */
APPLESTATIC int
nfsrvd_write(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	int i, cnt;
	u_int32_t *tl;
	mbuf_t mp;
	struct nfsvattr nva, forat;
	int aftat_ret = 1, retlen, len, error = 0, forat_ret = 1;
	int stable = NFSWRITE_FILESYNC;
	off_t off;
	struct nfsstate st, *stp = &st;
	struct nfslock lo, *lop = &lo;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, forat_ret, &forat, aftat_ret, &nva);
		goto out;
	}
	if (nd->nd_flag & ND_NFSV2) {
		NFSM_DISSECT(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
		retlen = len = fxdr_unsigned(int32_t, *tl);
	} else if (nd->nd_flag & ND_NFSV3) {
		NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 3;
		stable = fxdr_unsigned(int, *tl++);
		retlen = len = fxdr_unsigned(int32_t, *tl);
	} else {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID + 4 * NFSX_UNSIGNED);
		stp->ls_flags = (NFSLCK_CHECK | NFSLCK_WRITEACCESS);
		lop->lo_flags = NFSLCK_WRITE;
		stp->ls_ownerlen = 0;
		stp->ls_op = NULL;
		stp->ls_uid = nd->nd_cred->cr_uid;
		stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
		clientid.lval[0] = stp->ls_stateid.other[0] = *tl++;
		clientid.lval[1] = stp->ls_stateid.other[1] = *tl++;
		if (nd->nd_flag & ND_IMPLIEDCLID) {
			if (nd->nd_clientid.qval != clientid.qval)
				printf("EEK! multiple clids\n");
		} else {
			nd->nd_flag |= ND_IMPLIEDCLID;
			nd->nd_clientid.qval = clientid.qval;
		}
		stp->ls_stateid.other[2] = *tl++;
		off = fxdr_hyper(tl);
		lop->lo_first = off;
		tl += 2;
		stable = fxdr_unsigned(int, *tl++);
		retlen = len = fxdr_unsigned(int32_t, *tl);
		lop->lo_end = off + len;
		/*
		 * Paranoia, just in case it wraps around, which shouldn't
		 * ever happen anyhow.
		 */
		if (lop->lo_end < lop->lo_first)
			lop->lo_end = NFS64BITSSET;
	}

	/*
	 * Loop through the mbuf chain, counting how many mbufs are a
	 * part of this write operation, so the iovec size is known.
	 */
	cnt = 0;
	mp = nd->nd_md;
	i = NFSMTOD(mp, caddr_t) + mbuf_len(mp) - nd->nd_dpos;
	while (len > 0) {
		if (i > 0) {
			len -= i;
			cnt++;
		}
		mp = mbuf_next(mp);
		if (!mp) {
			if (len > 0) {
				error = EBADRPC;
				goto nfsmout;
			}
		} else
			i = mbuf_len(mp);
	}

	if (retlen > NFS_MAXDATA || retlen < 0)
		nd->nd_repstat = EIO;
	if (vnode_vtype(vp) != VREG && !nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3)
			nd->nd_repstat = EINVAL;
		else
			nd->nd_repstat = (vnode_vtype(vp) == VDIR) ? EISDIR :
			    EINVAL;
	}
	forat_ret = nfsvno_getattr(vp, &forat, nd->nd_cred, p, 1);
	if (!nd->nd_repstat)
		nd->nd_repstat = forat_ret;
	if (!nd->nd_repstat &&
	    (forat.na_uid != nd->nd_cred->cr_uid ||
	     NFSVNO_EXSTRICTACCESS(exp)))
		nd->nd_repstat = nfsvno_accchk(vp, VWRITE,
		    nd->nd_cred, exp, p,
		    NFSACCCHK_ALLOWOWNER, NFSACCCHK_VPISLOCKED, NULL);
	if ((nd->nd_flag & ND_NFSV4) && !nd->nd_repstat) {
		nd->nd_repstat = nfsrv_lockctrl(vp, &stp, &lop, NULL, clientid,
		    &stateid, exp, nd, p);
	}
	if (nd->nd_repstat) {
		vput(vp);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_wcc(nd, forat_ret, &forat, aftat_ret, &nva);
		goto out;
	}

	/*
	 * For NFS Version 2, it is not obvious what a write of zero length
	 * should do, but I might as well be consistent with Version 3,
	 * which is to return ok so long as there are no permission problems.
	 */
	if (retlen > 0) {
		nd->nd_repstat = nfsvno_write(vp, off, retlen, cnt, stable,
		    nd->nd_md, nd->nd_dpos, nd->nd_cred, p);
		error = nfsm_advance(nd, NFSM_RNDUP(retlen), -1);
		if (error)
			panic("nfsrv_write mbuf");
	}
	if (nd->nd_flag & ND_NFSV4)
		aftat_ret = 0;
	else
		aftat_ret = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
	vput(vp);
	if (!nd->nd_repstat)
		nd->nd_repstat = aftat_ret;
	if (nd->nd_flag & (ND_NFSV3 | ND_NFSV4)) {
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_wcc(nd, forat_ret, &forat, aftat_ret, &nva);
		if (nd->nd_repstat)
			goto out;
		NFSM_BUILD(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(retlen);
		/*
		 * If nfs_async is set, then pretend the write was FILESYNC.
		 * Warning: Doing this violates RFC1813 and runs a risk
		 * of data written by a client being lost when the server
		 * crashes/reboots.
		 */
		if (stable == NFSWRITE_UNSTABLE && nfs_async == 0)
			*tl++ = txdr_unsigned(stable);
		else
			*tl++ = txdr_unsigned(NFSWRITE_FILESYNC);
		/*
		 * Actually, there is no need to txdr these fields,
		 * but it may make the values more human readable,
		 * for debugging purposes.
		 */
		*tl++ = txdr_unsigned(nfsboottime.tv_sec);
		*tl = txdr_unsigned(nfsboottime.tv_usec);
	} else if (!nd->nd_repstat)
		nfsrv_fillattr(nd, &nva);

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs create service (creates regular files for V2 and V3. Spec. files for V2.)
 * now does a truncate to 0 length via. setattr if it already exists
 * The core creation routine has been extracted out into nfsrv_creatsub(),
 * so it can also be used by nfsrv_open() for V4.
 */
APPLESTATIC int
nfsrvd_create(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	struct nfsvattr nva, dirfor, diraft;
	struct nfsv2_sattr *sp;
	struct nameidata named;
	u_int32_t *tl;
	int error = 0, tsize, dirfor_ret = 1, diraft_ret = 1;
	int how = NFSCREATE_UNCHECKED, exclusive_flag = 0;
	NFSDEV_T rdev = 0;
	vnode_t vp = NULL, dirp = NULL;
	fhandle_t fh;
	char *bufp;
	u_long *hashp;
	enum vtype vtyp;
	int32_t cverf[2], tverf[2] = { 0, 0 };

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
		goto out;
	}
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, CREATE,
	    LOCKPARENT | LOCKLEAF | SAVESTART);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (error)
		goto nfsmout;
	if (!nd->nd_repstat) {
		NFSVNO_ATTRINIT(&nva);
		if (nd->nd_flag & ND_NFSV2) {
			NFSM_DISSECT(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
			vtyp = IFTOVT(fxdr_unsigned(u_int32_t, sp->sa_mode));
			if (vtyp == VNON)
				vtyp = VREG;
			NFSVNO_SETATTRVAL(&nva, type, vtyp);
			NFSVNO_SETATTRVAL(&nva, mode,
			    nfstov_mode(sp->sa_mode));
			switch (nva.na_type) {
			case VREG:
				tsize = fxdr_unsigned(int32_t, sp->sa_size);
				if (tsize != -1)
					NFSVNO_SETATTRVAL(&nva, size,
					    (u_quad_t)tsize);
				break;
			case VCHR:
			case VBLK:
			case VFIFO:
				rdev = fxdr_unsigned(NFSDEV_T, sp->sa_size);
				break;
			default:
				break;
			};
		} else {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			how = fxdr_unsigned(int, *tl);
			switch (how) {
			case NFSCREATE_GUARDED:
			case NFSCREATE_UNCHECKED:
				error = nfsrv_sattr(nd, &nva, NULL, NULL, p);
				if (error)
					goto nfsmout;
				break;
			case NFSCREATE_EXCLUSIVE:
				NFSM_DISSECT(tl, u_int32_t *, NFSX_VERF);
				cverf[0] = *tl++;
				cverf[1] = *tl;
				exclusive_flag = 1;
				break;
			};
			NFSVNO_SETATTRVAL(&nva, type, VREG);
		}
	}
	if (nd->nd_repstat) {
		nfsvno_relpathbuf(&named);
		if (nd->nd_flag & ND_NFSV3) {
			dirfor_ret = nfsvno_getattr(dp, &dirfor, nd->nd_cred,
			    p, 1);
			nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret,
			    &diraft);
		}
		vput(dp);
		goto out;
	}

	nd->nd_repstat = nfsvno_namei(nd, &named, dp, 1, exp, p, &dirp);
	if (dirp) {
		if (nd->nd_flag & ND_NFSV2) {
			vrele(dirp);
			dirp = NULL;
		} else {
			dirfor_ret = nfsvno_getattr(dirp, &dirfor, nd->nd_cred,
			    p, 0);
		}
	}
	if (nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret,
			    &diraft);
		if (dirp)
			vrele(dirp);
		goto out;
	}

	if (!(nd->nd_flag & ND_NFSV2)) {
		switch (how) {
		case NFSCREATE_GUARDED:
			if (named.ni_vp)
				nd->nd_repstat = EEXIST;
			break;
		case NFSCREATE_UNCHECKED:
			break;
		case NFSCREATE_EXCLUSIVE:
			if (named.ni_vp == NULL)
				NFSVNO_SETATTRVAL(&nva, mode, 0);
			break;
		};
	}

	/*
	 * Iff doesn't exist, create it
	 * otherwise just truncate to 0 length
	 *   should I set the mode too ?
	 */
	nd->nd_repstat = nfsvno_createsub(nd, &named, &vp, &nva,
	    &exclusive_flag, cverf, rdev, p, exp);

	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_getfh(vp, &fh, p);
		if (!nd->nd_repstat)
			nd->nd_repstat = nfsvno_getattr(vp, &nva, nd->nd_cred,
			    p, 1);
		vput(vp);
		if (!nd->nd_repstat) {
			tverf[0] = nva.na_atime.tv_sec;
			tverf[1] = nva.na_atime.tv_nsec;
		}
	}
	if (nd->nd_flag & ND_NFSV2) {
		if (!nd->nd_repstat) {
			(void) nfsm_fhtom(nd, (u_int8_t *)&fh, 0, 0);
			nfsrv_fillattr(nd, &nva);
		}
	} else {
		if (exclusive_flag && !nd->nd_repstat && (cverf[0] != tverf[0]
		    || cverf[1] != tverf[1]))
			nd->nd_repstat = EEXIST;
		diraft_ret = nfsvno_getattr(dirp, &diraft, nd->nd_cred, p, 0);
		vrele(dirp);
		if (!nd->nd_repstat) {
			(void) nfsm_fhtom(nd, (u_int8_t *)&fh, 0, 1);
			nfsrv_postopattr(nd, 0, &nva);
		}
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(dp);
	nfsvno_relpathbuf(&named);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs v3 mknod service (and v4 create)
 */
APPLESTATIC int
nfsrvd_mknod(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, vnode_t *vpp, fhandle_t *fhp, NFSPROC_T *p,
    struct nfsexstuff *exp)
{
	struct nfsvattr nva, dirfor, diraft;
	u_int32_t *tl;
	struct nameidata named;
	int error = 0, dirfor_ret = 1, diraft_ret = 1, pathlen;
	u_int32_t major, minor;
	enum vtype vtyp = VNON;
	nfstype nfs4type = NFNON;
	vnode_t vp, dirp = NULL;
	nfsattrbit_t attrbits;
	char *bufp = NULL, *pathcp = NULL;
	u_long *hashp, cnflags;
	NFSACL_T *aclp = NULL;

	NFSVNO_ATTRINIT(&nva);
	cnflags = (LOCKPARENT | SAVESTART);
	if (nd->nd_repstat) {
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
		goto out;
	}
#ifdef NFS4_ACL_EXTATTR_NAME
	aclp = acl_alloc(M_WAITOK);
	aclp->acl_cnt = 0;
#endif

	/*
	 * For V4, the creation stuff is here, Yuck!
	 */
	if (nd->nd_flag & ND_NFSV4) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		vtyp = nfsv34tov_type(*tl);
		nfs4type = fxdr_unsigned(nfstype, *tl);
		switch (nfs4type) {
		case NFLNK:
			error = nfsvno_getsymlink(nd, &nva, p, &pathcp,
			    &pathlen);
			if (error)
				goto nfsmout;
			break;
		case NFCHR:
		case NFBLK:
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			major = fxdr_unsigned(u_int32_t, *tl++);
			minor = fxdr_unsigned(u_int32_t, *tl);
			nva.na_rdev = NFSMAKEDEV(major, minor);
			break;
		case NFSOCK:
		case NFFIFO:
			break;
		case NFDIR:
			cnflags = (LOCKPARENT | SAVENAME);
			break;
		default:
			nd->nd_repstat = NFSERR_BADTYPE;
			vrele(dp);
#ifdef NFS4_ACL_EXTATTR_NAME
			acl_free(aclp);
#endif
			goto out;
		}
	}
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, CREATE, cnflags);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (error)
		goto nfsmout;
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3) {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			vtyp = nfsv34tov_type(*tl);
		}
		error = nfsrv_sattr(nd, &nva, &attrbits, aclp, p);
		if (error)
			goto nfsmout;
		nva.na_type = vtyp;
		if (!nd->nd_repstat && (nd->nd_flag & ND_NFSV3) &&
		    (vtyp == VCHR || vtyp == VBLK)) {
			NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			major = fxdr_unsigned(u_int32_t, *tl++);
			minor = fxdr_unsigned(u_int32_t, *tl);
			nva.na_rdev = NFSMAKEDEV(major, minor);
		}
	}

	dirfor_ret = nfsvno_getattr(dp, &dirfor, nd->nd_cred, p, 0);
	if (!nd->nd_repstat && (nd->nd_flag & ND_NFSV4)) {
		if (!dirfor_ret && NFSVNO_ISSETGID(&nva) &&
		    dirfor.na_gid == nva.na_gid)
			NFSVNO_UNSET(&nva, gid);
		nd->nd_repstat = nfsrv_checkuidgid(nd, &nva);
	}
	if (nd->nd_repstat) {
		vrele(dp);
#ifdef NFS4_ACL_EXTATTR_NAME
		acl_free(aclp);
#endif
		nfsvno_relpathbuf(&named);
		if (pathcp)
			FREE(pathcp, M_TEMP);
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret,
			    &diraft);
		goto out;
	}

	/*
	 * Yuck! For V4, mkdir and link are here and some V4 clients don't fill
	 * in va_mode, so we'll have to set a default here.
	 */
	if (NFSVNO_NOTSETMODE(&nva)) {
		if (vtyp == VLNK)
			nva.na_mode = 0755;
		else
			nva.na_mode = 0400;
	}

	if (vtyp == VDIR)
		named.ni_cnd.cn_flags |= WILLBEDIR;
	nd->nd_repstat = nfsvno_namei(nd, &named, dp, 0, exp, p, &dirp);
	if (nd->nd_repstat) {
		if (dirp) {
			if (nd->nd_flag & ND_NFSV3)
				dirfor_ret = nfsvno_getattr(dirp, &dirfor,
				    nd->nd_cred, p, 0);
			vrele(dirp);
		}
#ifdef NFS4_ACL_EXTATTR_NAME
		acl_free(aclp);
#endif
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret,
			    &diraft);
		goto out;
	}
	if (dirp)
		dirfor_ret = nfsvno_getattr(dirp, &dirfor, nd->nd_cred, p, 0);

	if ((nd->nd_flag & ND_NFSV4) && (vtyp == VDIR || vtyp == VLNK)) {
		if (vtyp == VDIR) {
			nfsrvd_mkdirsub(nd, &named, &nva, fhp, vpp, dirp,
			    &dirfor, &diraft, &diraft_ret, &attrbits, aclp, p,
			    exp);
#ifdef NFS4_ACL_EXTATTR_NAME
			acl_free(aclp);
#endif
			goto out;
		} else if (vtyp == VLNK) {
			nfsrvd_symlinksub(nd, &named, &nva, fhp, vpp, dirp,
			    &dirfor, &diraft, &diraft_ret, &attrbits,
			    aclp, p, exp, pathcp, pathlen);
#ifdef NFS4_ACL_EXTATTR_NAME
			acl_free(aclp);
#endif
			FREE(pathcp, M_TEMP);
			goto out;
		}
	}

	nd->nd_repstat = nfsvno_mknod(&named, &nva, nd->nd_cred, p);
	if (!nd->nd_repstat) {
		vp = named.ni_vp;
		nfsrv_fixattr(nd, vp, &nva, aclp, p, &attrbits, exp);
		nd->nd_repstat = nfsvno_getfh(vp, fhp, p);
		if ((nd->nd_flag & ND_NFSV3) && !nd->nd_repstat)
			nd->nd_repstat = nfsvno_getattr(vp, &nva, nd->nd_cred,
			    p, 1);
		if (vpp != NULL && nd->nd_repstat == 0) {
			NFSVOPUNLOCK(vp, 0);
			*vpp = vp;
		} else
			vput(vp);
	}

	diraft_ret = nfsvno_getattr(dirp, &diraft, nd->nd_cred, p, 0);
	vrele(dirp);
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3) {
			(void) nfsm_fhtom(nd, (u_int8_t *)fhp, 0, 1);
			nfsrv_postopattr(nd, 0, &nva);
		} else {
			NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			*tl++ = newnfs_false;
			txdr_hyper(dirfor.na_filerev, tl);
			tl += 2;
			txdr_hyper(diraft.na_filerev, tl);
			(void) nfsrv_putattrbit(nd, &attrbits);
		}
	}
	if (nd->nd_flag & ND_NFSV3)
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
#ifdef NFS4_ACL_EXTATTR_NAME
	acl_free(aclp);
#endif

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vrele(dp);
#ifdef NFS4_ACL_EXTATTR_NAME
	acl_free(aclp);
#endif
	if (bufp)
		nfsvno_relpathbuf(&named);
	if (pathcp)
		FREE(pathcp, M_TEMP);

	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs remove service
 */
APPLESTATIC int
nfsrvd_remove(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	struct nameidata named;
	u_int32_t *tl;
	int error = 0, dirfor_ret = 1, diraft_ret = 1;
	vnode_t dirp = NULL;
	struct nfsvattr dirfor, diraft;
	char *bufp;
	u_long *hashp;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
		goto out;
	}
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, DELETE,
	    LOCKPARENT | LOCKLEAF);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (error) {
		vput(dp);
		nfsvno_relpathbuf(&named);
		goto out;
	}
	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_namei(nd, &named, dp, 1, exp, p, &dirp);
	} else {
		vput(dp);
		nfsvno_relpathbuf(&named);
	}
	if (dirp) {
		if (!(nd->nd_flag & ND_NFSV2)) {
			dirfor_ret = nfsvno_getattr(dirp, &dirfor,
			    nd->nd_cred, p, 0);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV4) {
			if (vnode_vtype(named.ni_vp) == VDIR)
				nd->nd_repstat = nfsvno_rmdirsub(&named, 1,
				    nd->nd_cred, p, exp);
			else
				nd->nd_repstat = nfsvno_removesub(&named, 1,
				    nd->nd_cred, p, exp);
		} else if (nd->nd_procnum == NFSPROC_RMDIR) {
			nd->nd_repstat = nfsvno_rmdirsub(&named, 0,
			    nd->nd_cred, p, exp);
		} else {
			nd->nd_repstat = nfsvno_removesub(&named, 0,
			    nd->nd_cred, p, exp);
		}
	}
	if (!(nd->nd_flag & ND_NFSV2)) {
		if (dirp) {
			diraft_ret = nfsvno_getattr(dirp, &diraft, nd->nd_cred,
			    p, 0);
			vrele(dirp);
		}
		if (nd->nd_flag & ND_NFSV3) {
			nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret,
			    &diraft);
		} else if (!nd->nd_repstat) {
			NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
			*tl++ = newnfs_false;
			txdr_hyper(dirfor.na_filerev, tl);
			tl += 2;
			txdr_hyper(diraft.na_filerev, tl);
		}
	}

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs rename service
 */
APPLESTATIC int
nfsrvd_rename(struct nfsrv_descript *nd, int isdgram,
    vnode_t dp, vnode_t todp, NFSPROC_T *p, struct nfsexstuff *exp,
    struct nfsexstuff *toexp)
{
	u_int32_t *tl;
	int error = 0, fdirfor_ret = 1, fdiraft_ret = 1;
	int tdirfor_ret = 1, tdiraft_ret = 1;
	struct nameidata fromnd, tond;
	vnode_t fdirp = NULL, tdirp = NULL, tdp = NULL;
	struct nfsvattr fdirfor, fdiraft, tdirfor, tdiraft;
	struct nfsexstuff tnes;
	struct nfsrvfh tfh;
	char *bufp, *tbufp = NULL;
	u_long *hashp;
	fhandle_t fh;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsrv_wcc(nd, tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
		goto out;
	}
	if (!(nd->nd_flag & ND_NFSV2))
		fdirfor_ret = nfsvno_getattr(dp, &fdirfor, nd->nd_cred, p, 1);
	tond.ni_cnd.cn_nameiop = 0;
	tond.ni_startdir = NULL;
	NFSNAMEICNDSET(&fromnd.ni_cnd, nd->nd_cred, DELETE, WANTPARENT | SAVESTART);
	nfsvno_setpathbuf(&fromnd, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &fromnd.ni_pathlen);
	if (error) {
		vput(dp);
		if (todp)
			vrele(todp);
		nfsvno_relpathbuf(&fromnd);
		goto out;
	}
	if (nd->nd_flag & ND_NFSV4) {
		tdp = todp;
		tnes = *toexp;
		tdirfor_ret = nfsvno_getattr(tdp, &tdirfor, nd->nd_cred, p, 0);
	} else {
		tfh.nfsrvfh_len = 0;
		error = nfsrv_mtofh(nd, &tfh);
		if (error == 0)
			error = nfsvno_getfh(dp, &fh, p);
		if (error) {
			vput(dp);
			/* todp is always NULL except NFSv4 */
			nfsvno_relpathbuf(&fromnd);
			goto out;
		}

		/* If this is the same file handle, just VREF() the vnode. */
		if (tfh.nfsrvfh_len == NFSX_MYFH &&
		    !NFSBCMP(tfh.nfsrvfh_data, &fh, NFSX_MYFH)) {
			VREF(dp);
			tdp = dp;
			tnes = *exp;
			tdirfor_ret = nfsvno_getattr(tdp, &tdirfor, nd->nd_cred,
			    p, 1);
		} else {
			nd->nd_cred->cr_uid = nd->nd_saveduid;
			nfsd_fhtovp(nd, &tfh, LK_EXCLUSIVE, &tdp, &tnes, NULL,
			    0, p);
			if (tdp) {
				tdirfor_ret = nfsvno_getattr(tdp, &tdirfor,
				    nd->nd_cred, p, 1);
				NFSVOPUNLOCK(tdp, 0);
			}
		}
	}
	NFSNAMEICNDSET(&tond.ni_cnd, nd->nd_cred, RENAME, LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART);
	nfsvno_setpathbuf(&tond, &tbufp, &hashp);
	if (!nd->nd_repstat) {
		error = nfsrv_parsename(nd, tbufp, hashp, &tond.ni_pathlen);
		if (error) {
			if (tdp)
				vrele(tdp);
			vput(dp);
			nfsvno_relpathbuf(&fromnd);
			nfsvno_relpathbuf(&tond);
			goto out;
		}
	}
	if (nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3) {
			nfsrv_wcc(nd, fdirfor_ret, &fdirfor, fdiraft_ret,
			    &fdiraft);
			nfsrv_wcc(nd, tdirfor_ret, &tdirfor, tdiraft_ret,
			    &tdiraft);
		}
		if (tdp)
			vrele(tdp);
		vput(dp);
		nfsvno_relpathbuf(&fromnd);
		nfsvno_relpathbuf(&tond);
		goto out;
	}

	/*
	 * Done parsing, now down to business.
	 */
	nd->nd_repstat = nfsvno_namei(nd, &fromnd, dp, 1, exp, p, &fdirp);
	if (nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV3) {
			nfsrv_wcc(nd, fdirfor_ret, &fdirfor, fdiraft_ret,
			    &fdiraft);
			nfsrv_wcc(nd, tdirfor_ret, &tdirfor, tdiraft_ret,
			    &tdiraft);
		}
		if (fdirp)
			vrele(fdirp);
		if (tdp)
			vrele(tdp);
		nfsvno_relpathbuf(&tond);
		goto out;
	}
	if (vnode_vtype(fromnd.ni_vp) == VDIR)
		tond.ni_cnd.cn_flags |= WILLBEDIR;
	nd->nd_repstat = nfsvno_namei(nd, &tond, tdp, 0, &tnes, p, &tdirp);
	nd->nd_repstat = nfsvno_rename(&fromnd, &tond, nd->nd_repstat,
	    nd->nd_flag, nd->nd_cred, p);
	if (fdirp)
		fdiraft_ret = nfsvno_getattr(fdirp, &fdiraft, nd->nd_cred, p,
		    0);
	if (tdirp)
		tdiraft_ret = nfsvno_getattr(tdirp, &tdiraft, nd->nd_cred, p,
		    0);
	if (fdirp)
		vrele(fdirp);
	if (tdirp)
		vrele(tdirp);
	if (nd->nd_flag & ND_NFSV3) {
		nfsrv_wcc(nd, fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsrv_wcc(nd, tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
	} else if ((nd->nd_flag & ND_NFSV4) && !nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, 10 * NFSX_UNSIGNED);
		*tl++ = newnfs_false;
		txdr_hyper(fdirfor.na_filerev, tl);
		tl += 2;
		txdr_hyper(fdiraft.na_filerev, tl);
		tl += 2;
		*tl++ = newnfs_false;
		txdr_hyper(tdirfor.na_filerev, tl);
		tl += 2;
		txdr_hyper(tdiraft.na_filerev, tl);
	}

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs link service
 */
APPLESTATIC int
nfsrvd_link(struct nfsrv_descript *nd, int isdgram,
    vnode_t vp, vnode_t tovp, NFSPROC_T *p, struct nfsexstuff *exp,
    struct nfsexstuff *toexp)
{
	struct nameidata named;
	u_int32_t *tl;
	int error = 0, dirfor_ret = 1, diraft_ret = 1, getret = 1;
	vnode_t dirp = NULL, dp = NULL;
	struct nfsvattr dirfor, diraft, at;
	struct nfsexstuff tnes;
	struct nfsrvfh dfh;
	char *bufp;
	u_long *hashp;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &at);
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
		goto out;
	}
	NFSVOPUNLOCK(vp, 0);
	if (vnode_vtype(vp) == VDIR) {
		if (nd->nd_flag & ND_NFSV4)
			nd->nd_repstat = NFSERR_ISDIR;
		else
			nd->nd_repstat = NFSERR_INVAL;
		if (tovp)
			vrele(tovp);
	} else if (vnode_vtype(vp) == VLNK) {
		if (nd->nd_flag & ND_NFSV2)
			nd->nd_repstat = NFSERR_INVAL;
		else
			nd->nd_repstat = NFSERR_NOTSUPP;
		if (tovp)
			vrele(tovp);
	}
	if (!nd->nd_repstat) {
		if (nd->nd_flag & ND_NFSV4) {
			dp = tovp;
			tnes = *toexp;
		} else {
			error = nfsrv_mtofh(nd, &dfh);
			if (error) {
				vrele(vp);
				/* tovp is always NULL unless NFSv4 */
				goto out;
			}
			nfsd_fhtovp(nd, &dfh, LK_EXCLUSIVE, &dp, &tnes, NULL, 0,
			    p);
			if (dp)
				NFSVOPUNLOCK(dp, 0);
		}
	}
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, CREATE,
	    LOCKPARENT | SAVENAME);
	if (!nd->nd_repstat) {
		nfsvno_setpathbuf(&named, &bufp, &hashp);
		error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
		if (error) {
			vrele(vp);
			if (dp)
				vrele(dp);
			nfsvno_relpathbuf(&named);
			goto out;
		}
		if (!nd->nd_repstat) {
			nd->nd_repstat = nfsvno_namei(nd, &named, dp, 0, &tnes,
			    p, &dirp);
		} else {
			if (dp)
				vrele(dp);
			nfsvno_relpathbuf(&named);
		}
	}
	if (dirp) {
		if (nd->nd_flag & ND_NFSV2) {
			vrele(dirp);
			dirp = NULL;
		} else {
			dirfor_ret = nfsvno_getattr(dirp, &dirfor,
			    nd->nd_cred, p, 0);
		}
	}
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_link(&named, vp, nd->nd_cred, p, exp);
	if (nd->nd_flag & ND_NFSV3)
		getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 0);
	if (dirp) {
		diraft_ret = nfsvno_getattr(dirp, &diraft, nd->nd_cred, p, 0);
		vrele(dirp);
	}
	vrele(vp);
	if (nd->nd_flag & ND_NFSV3) {
		nfsrv_postopattr(nd, getret, &at);
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
	} else if ((nd->nd_flag & ND_NFSV4) && !nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		*tl++ = newnfs_false;
		txdr_hyper(dirfor.na_filerev, tl);
		tl += 2;
		txdr_hyper(diraft.na_filerev, tl);
	}

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs symbolic link service
 */
APPLESTATIC int
nfsrvd_symlink(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, vnode_t *vpp, fhandle_t *fhp, NFSPROC_T *p,
    struct nfsexstuff *exp)
{
	struct nfsvattr nva, dirfor, diraft;
	struct nameidata named;
	int error = 0, dirfor_ret = 1, diraft_ret = 1, pathlen;
	vnode_t dirp = NULL;
	char *bufp, *pathcp = NULL;
	u_long *hashp;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
		goto out;
	}
	if (vpp)
		*vpp = NULL;
	NFSVNO_ATTRINIT(&nva);
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, CREATE,
	    LOCKPARENT | SAVESTART);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (!error && !nd->nd_repstat)
		error = nfsvno_getsymlink(nd, &nva, p, &pathcp, &pathlen);
	if (error) {
		vrele(dp);
		nfsvno_relpathbuf(&named);
		goto out;
	}
	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_namei(nd, &named, dp, 0, exp, p, &dirp);
	} else {
		vrele(dp);
		nfsvno_relpathbuf(&named);
	}
	if (dirp != NULL && !(nd->nd_flag & ND_NFSV3)) {
		vrele(dirp);
		dirp = NULL;
	}

	/*
	 * And call nfsrvd_symlinksub() to do the common code. It will
	 * return EBADRPC upon a parsing error, 0 otherwise.
	 */
	if (!nd->nd_repstat) {
		if (dirp != NULL)
			dirfor_ret = nfsvno_getattr(dirp, &dirfor, nd->nd_cred,
			    p, 0);
		nfsrvd_symlinksub(nd, &named, &nva, fhp, vpp, dirp,
		    &dirfor, &diraft, &diraft_ret, NULL, NULL, p, exp,
		    pathcp, pathlen);
	} else if (dirp != NULL) {
		dirfor_ret = nfsvno_getattr(dirp, &dirfor, nd->nd_cred, p, 0);
		vrele(dirp);
	}
	if (pathcp)
		FREE(pathcp, M_TEMP);

	if (nd->nd_flag & ND_NFSV3) {
		if (!nd->nd_repstat) {
			(void) nfsm_fhtom(nd, (u_int8_t *)fhp, 0, 1);
			nfsrv_postopattr(nd, 0, &nva);
		}
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
	}

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * Common code for creating a symbolic link.
 */
static void
nfsrvd_symlinksub(struct nfsrv_descript *nd, struct nameidata *ndp,
    struct nfsvattr *nvap, fhandle_t *fhp, vnode_t *vpp,
    vnode_t dirp, struct nfsvattr *dirforp, struct nfsvattr *diraftp,
    int *diraft_retp, nfsattrbit_t *attrbitp,
    NFSACL_T *aclp, NFSPROC_T *p, struct nfsexstuff *exp, char *pathcp,
    int pathlen)
{
	u_int32_t *tl;

	nd->nd_repstat = nfsvno_symlink(ndp, nvap, pathcp, pathlen,
	    !(nd->nd_flag & ND_NFSV2), nd->nd_saveduid, nd->nd_cred, p, exp);
	if (!nd->nd_repstat && !(nd->nd_flag & ND_NFSV2)) {
		nfsrv_fixattr(nd, ndp->ni_vp, nvap, aclp, p, attrbitp, exp);
		if (nd->nd_flag & ND_NFSV3) {
			nd->nd_repstat = nfsvno_getfh(ndp->ni_vp, fhp, p);
			if (!nd->nd_repstat)
				nd->nd_repstat = nfsvno_getattr(ndp->ni_vp,
				    nvap, nd->nd_cred, p, 1);
		}
		if (vpp != NULL && nd->nd_repstat == 0) {
			NFSVOPUNLOCK(ndp->ni_vp, 0);
			*vpp = ndp->ni_vp;
		} else
			vput(ndp->ni_vp);
	}
	if (dirp) {
		*diraft_retp = nfsvno_getattr(dirp, diraftp, nd->nd_cred, p, 0);
		vrele(dirp);
	}
	if ((nd->nd_flag & ND_NFSV4) && !nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		*tl++ = newnfs_false;
		txdr_hyper(dirforp->na_filerev, tl);
		tl += 2;
		txdr_hyper(diraftp->na_filerev, tl);
		(void) nfsrv_putattrbit(nd, attrbitp);
	}

	NFSEXITCODE2(0, nd);
}

/*
 * nfs mkdir service
 */
APPLESTATIC int
nfsrvd_mkdir(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, vnode_t *vpp, fhandle_t *fhp, NFSPROC_T *p,
    struct nfsexstuff *exp)
{
	struct nfsvattr nva, dirfor, diraft;
	struct nameidata named;
	u_int32_t *tl;
	int error = 0, dirfor_ret = 1, diraft_ret = 1;
	vnode_t dirp = NULL;
	char *bufp;
	u_long *hashp;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
		goto out;
	}
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, CREATE,
	    LOCKPARENT | SAVENAME);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (error)
		goto nfsmout;
	if (!nd->nd_repstat) {
		NFSVNO_ATTRINIT(&nva);
		if (nd->nd_flag & ND_NFSV3) {
			error = nfsrv_sattr(nd, &nva, NULL, NULL, p);
			if (error)
				goto nfsmout;
		} else {
			NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
			nva.na_mode = nfstov_mode(*tl++);
		}
	}
	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_namei(nd, &named, dp, 0, exp, p, &dirp);
	} else {
		vrele(dp);
		nfsvno_relpathbuf(&named);
	}
	if (dirp != NULL && !(nd->nd_flag & ND_NFSV3)) {
		vrele(dirp);
		dirp = NULL;
	}
	if (nd->nd_repstat) {
		if (dirp != NULL) {
			dirfor_ret = nfsvno_getattr(dirp, &dirfor, nd->nd_cred,
			    p, 0);
			vrele(dirp);
		}
		if (nd->nd_flag & ND_NFSV3)
			nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret,
			    &diraft);
		goto out;
	}
	if (dirp != NULL)
		dirfor_ret = nfsvno_getattr(dirp, &dirfor, nd->nd_cred, p, 0);

	/*
	 * Call nfsrvd_mkdirsub() for the code common to V4 as well.
	 */
	nfsrvd_mkdirsub(nd, &named, &nva, fhp, vpp, dirp, &dirfor, &diraft,
	    &diraft_ret, NULL, NULL, p, exp);

	if (nd->nd_flag & ND_NFSV3) {
		if (!nd->nd_repstat) {
			(void) nfsm_fhtom(nd, (u_int8_t *)fhp, 0, 1);
			nfsrv_postopattr(nd, 0, &nva);
		}
		nfsrv_wcc(nd, dirfor_ret, &dirfor, diraft_ret, &diraft);
	} else if (!nd->nd_repstat) {
		(void) nfsm_fhtom(nd, (u_int8_t *)fhp, 0, 0);
		nfsrv_fillattr(nd, &nva);
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vrele(dp);
	nfsvno_relpathbuf(&named);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * Code common to mkdir for V2,3 and 4.
 */
static void
nfsrvd_mkdirsub(struct nfsrv_descript *nd, struct nameidata *ndp,
    struct nfsvattr *nvap, fhandle_t *fhp, vnode_t *vpp,
    vnode_t dirp, struct nfsvattr *dirforp, struct nfsvattr *diraftp,
    int *diraft_retp, nfsattrbit_t *attrbitp, NFSACL_T *aclp,
    NFSPROC_T *p, struct nfsexstuff *exp)
{
	vnode_t vp;
	u_int32_t *tl;

	NFSVNO_SETATTRVAL(nvap, type, VDIR);
	nd->nd_repstat = nfsvno_mkdir(ndp, nvap, nd->nd_saveduid,
	    nd->nd_cred, p, exp);
	if (!nd->nd_repstat) {
		vp = ndp->ni_vp;
		nfsrv_fixattr(nd, vp, nvap, aclp, p, attrbitp, exp);
		nd->nd_repstat = nfsvno_getfh(vp, fhp, p);
		if (!(nd->nd_flag & ND_NFSV4) && !nd->nd_repstat)
			nd->nd_repstat = nfsvno_getattr(vp, nvap, nd->nd_cred,
			    p, 1);
		if (vpp && !nd->nd_repstat) {
			NFSVOPUNLOCK(vp, 0);
			*vpp = vp;
		} else {
			vput(vp);
		}
	}
	if (dirp) {
		*diraft_retp = nfsvno_getattr(dirp, diraftp, nd->nd_cred, p, 0);
		vrele(dirp);
	}
	if ((nd->nd_flag & ND_NFSV4) && !nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		*tl++ = newnfs_false;
		txdr_hyper(dirforp->na_filerev, tl);
		tl += 2;
		txdr_hyper(diraftp->na_filerev, tl);
		(void) nfsrv_putattrbit(nd, attrbitp);
	}

	NFSEXITCODE2(0, nd);
}

/*
 * nfs commit service
 */
APPLESTATIC int
nfsrvd_commit(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	struct nfsvattr bfor, aft;
	u_int32_t *tl;
	int error = 0, for_ret = 1, aft_ret = 1, cnt;
	u_int64_t off;

	if (nd->nd_repstat) {
		nfsrv_wcc(nd, for_ret, &bfor, aft_ret, &aft);
		goto out;
	}
	NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	/*
	 * XXX At this time VOP_FSYNC() does not accept offset and byte
	 * count parameters, so these arguments are useless (someday maybe).
	 */
	off = fxdr_hyper(tl);
	tl += 2;
	cnt = fxdr_unsigned(int, *tl);
	if (nd->nd_flag & ND_NFSV3)
		for_ret = nfsvno_getattr(vp, &bfor, nd->nd_cred, p, 1);
	nd->nd_repstat = nfsvno_fsync(vp, off, cnt, nd->nd_cred, p);
	if (nd->nd_flag & ND_NFSV3) {
		aft_ret = nfsvno_getattr(vp, &aft, nd->nd_cred, p, 1);
		nfsrv_wcc(nd, for_ret, &bfor, aft_ret, &aft);
	}
	vput(vp);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_VERF);
		*tl++ = txdr_unsigned(nfsboottime.tv_sec);
		*tl = txdr_unsigned(nfsboottime.tv_usec);
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs statfs service
 */
APPLESTATIC int
nfsrvd_statfs(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	struct statfs *sf;
	u_int32_t *tl;
	int getret = 1;
	struct nfsvattr at;
	struct statfs sfs;
	u_quad_t tval;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &at);
		goto out;
	}
	sf = &sfs;
	nd->nd_repstat = nfsvno_statfs(vp, sf);
	getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 1);
	vput(vp);
	if (nd->nd_flag & ND_NFSV3)
		nfsrv_postopattr(nd, getret, &at);
	if (nd->nd_repstat)
		goto out;
	if (nd->nd_flag & ND_NFSV2) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_V2STATFS);
		*tl++ = txdr_unsigned(NFS_V2MAXDATA);
		*tl++ = txdr_unsigned(sf->f_bsize);
		*tl++ = txdr_unsigned(sf->f_blocks);
		*tl++ = txdr_unsigned(sf->f_bfree);
		*tl = txdr_unsigned(sf->f_bavail);
	} else {
		NFSM_BUILD(tl, u_int32_t *, NFSX_V3STATFS);
		tval = (u_quad_t)sf->f_blocks;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, tl); tl += 2;
		tval = (u_quad_t)sf->f_bfree;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, tl); tl += 2;
		tval = (u_quad_t)sf->f_bavail;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, tl); tl += 2;
		tval = (u_quad_t)sf->f_files;
		txdr_hyper(tval, tl); tl += 2;
		tval = (u_quad_t)sf->f_ffree;
		txdr_hyper(tval, tl); tl += 2;
		tval = (u_quad_t)sf->f_ffree;
		txdr_hyper(tval, tl); tl += 2;
		*tl = 0;
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
}

/*
 * nfs fsinfo service
 */
APPLESTATIC int
nfsrvd_fsinfo(struct nfsrv_descript *nd, int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	struct nfsfsinfo fs;
	int getret = 1;
	struct nfsvattr at;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &at);
		goto out;
	}
	getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 1);
	nfsvno_getfs(&fs, isdgram);
	vput(vp);
	nfsrv_postopattr(nd, getret, &at);
	NFSM_BUILD(tl, u_int32_t *, NFSX_V3FSINFO);
	*tl++ = txdr_unsigned(fs.fs_rtmax);
	*tl++ = txdr_unsigned(fs.fs_rtpref);
	*tl++ = txdr_unsigned(fs.fs_rtmult);
	*tl++ = txdr_unsigned(fs.fs_wtmax);
	*tl++ = txdr_unsigned(fs.fs_wtpref);
	*tl++ = txdr_unsigned(fs.fs_wtmult);
	*tl++ = txdr_unsigned(fs.fs_dtpref);
	txdr_hyper(fs.fs_maxfilesize, tl);
	tl += 2;
	txdr_nfsv3time(&fs.fs_timedelta, tl);
	tl += 2;
	*tl = txdr_unsigned(fs.fs_properties);

out:
	NFSEXITCODE2(0, nd);
	return (0);
}

/*
 * nfs pathconf service
 */
APPLESTATIC int
nfsrvd_pathconf(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	struct nfsv3_pathconf *pc;
	int getret = 1;
	register_t linkmax, namemax, chownres, notrunc;
	struct nfsvattr at;

	if (nd->nd_repstat) {
		nfsrv_postopattr(nd, getret, &at);
		goto out;
	}
	nd->nd_repstat = nfsvno_pathconf(vp, _PC_LINK_MAX, &linkmax,
	    nd->nd_cred, p);
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_pathconf(vp, _PC_NAME_MAX, &namemax,
		    nd->nd_cred, p);
	if (!nd->nd_repstat)
		nd->nd_repstat=nfsvno_pathconf(vp, _PC_CHOWN_RESTRICTED,
		    &chownres, nd->nd_cred, p);
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_pathconf(vp, _PC_NO_TRUNC, &notrunc,
		    nd->nd_cred, p);
	getret = nfsvno_getattr(vp, &at, nd->nd_cred, p, 1);
	vput(vp);
	nfsrv_postopattr(nd, getret, &at);
	if (!nd->nd_repstat) {
		NFSM_BUILD(pc, struct nfsv3_pathconf *, NFSX_V3PATHCONF);
		pc->pc_linkmax = txdr_unsigned(linkmax);
		pc->pc_namemax = txdr_unsigned(namemax);
		pc->pc_notrunc = txdr_unsigned(notrunc);
		pc->pc_chownrestricted = txdr_unsigned(chownres);

		/*
		 * These should probably be supported by VOP_PATHCONF(), but
		 * until msdosfs is exportable (why would you want to?), the
		 * Unix defaults should be ok.
		 */
		pc->pc_caseinsensitive = newnfs_false;
		pc->pc_casepreserving = newnfs_true;
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
}

/*
 * nfsv4 lock service
 */
APPLESTATIC int
nfsrvd_lock(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int i;
	struct nfsstate *stp = NULL;
	struct nfslock *lop;
	struct nfslockconflict cf;
	int error = 0;
	u_short flags = NFSLCK_LOCK, lflags;
	u_int64_t offset, len;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	NFSM_DISSECT(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
	i = fxdr_unsigned(int, *tl++);
	switch (i) {
	case NFSV4LOCKT_READW:
		flags |= NFSLCK_BLOCKING;
	case NFSV4LOCKT_READ:
		lflags = NFSLCK_READ;
		break;
	case NFSV4LOCKT_WRITEW:
		flags |= NFSLCK_BLOCKING;
	case NFSV4LOCKT_WRITE:
		lflags = NFSLCK_WRITE;
		break;
	default:
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	};
	if (*tl++ == newnfs_true)
		flags |= NFSLCK_RECLAIM;
	offset = fxdr_hyper(tl);
	tl += 2;
	len = fxdr_hyper(tl);
	tl += 2;
	if (*tl == newnfs_true)
		flags |= NFSLCK_OPENTOLOCK;
	if (flags & NFSLCK_OPENTOLOCK) {
		NFSM_DISSECT(tl, u_int32_t *, 5 * NFSX_UNSIGNED + NFSX_STATEID);
		i = fxdr_unsigned(int, *(tl+4+(NFSX_STATEID / NFSX_UNSIGNED)));
		if (i <= 0 || i > NFSV4_OPAQUELIMIT) {
			nd->nd_repstat = NFSERR_BADXDR;
			goto nfsmout;
		}
		MALLOC(stp, struct nfsstate *, sizeof (struct nfsstate) + i,
			M_NFSDSTATE, M_WAITOK);
		stp->ls_ownerlen = i;
		stp->ls_op = nd->nd_rp;
		stp->ls_seq = fxdr_unsigned(int, *tl++);
		stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
		NFSBCOPY((caddr_t)tl, (caddr_t)stp->ls_stateid.other,
			NFSX_STATEIDOTHER);
		tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
		stp->ls_opentolockseq = fxdr_unsigned(int, *tl++);
		clientid.lval[0] = *tl++;
		clientid.lval[1] = *tl++;
		if (nd->nd_flag & ND_IMPLIEDCLID) {
			if (nd->nd_clientid.qval != clientid.qval)
				printf("EEK! multiple clids\n");
		} else {
			nd->nd_flag |= ND_IMPLIEDCLID;
			nd->nd_clientid.qval = clientid.qval;
		}
		error = nfsrv_mtostr(nd, stp->ls_owner, stp->ls_ownerlen);
		if (error)
			goto nfsmout;
	} else {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID + NFSX_UNSIGNED);
		MALLOC(stp, struct nfsstate *, sizeof (struct nfsstate),
			M_NFSDSTATE, M_WAITOK);
		stp->ls_ownerlen = 0;
		stp->ls_op = nd->nd_rp;
		stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
		NFSBCOPY((caddr_t)tl, (caddr_t)stp->ls_stateid.other,
			NFSX_STATEIDOTHER);
		tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
		stp->ls_seq = fxdr_unsigned(int, *tl);
		clientid.lval[0] = stp->ls_stateid.other[0];
		clientid.lval[1] = stp->ls_stateid.other[1];
		if (nd->nd_flag & ND_IMPLIEDCLID) {
			if (nd->nd_clientid.qval != clientid.qval)
				printf("EEK! multiple clids\n");
		} else {
			nd->nd_flag |= ND_IMPLIEDCLID;
			nd->nd_clientid.qval = clientid.qval;
		}
	}
	MALLOC(lop, struct nfslock *, sizeof (struct nfslock),
		M_NFSDLOCK, M_WAITOK);
	lop->lo_first = offset;
	if (len == NFS64BITSSET) {
		lop->lo_end = NFS64BITSSET;
	} else {
		lop->lo_end = offset + len;
		if (lop->lo_end <= lop->lo_first)
			nd->nd_repstat = NFSERR_INVAL;
	}
	lop->lo_flags = lflags;
	stp->ls_flags = flags;
	stp->ls_uid = nd->nd_cred->cr_uid;

	/*
	 * Do basic access checking.
	 */
	if (!nd->nd_repstat && vnode_vtype(vp) != VREG) {
	    if (vnode_vtype(vp) == VDIR)
		nd->nd_repstat = NFSERR_ISDIR;
	    else
		nd->nd_repstat = NFSERR_INVAL;
	}
	if (!nd->nd_repstat) {
	    if (lflags & NFSLCK_WRITE) {
		nd->nd_repstat = nfsvno_accchk(vp, VWRITE,
		    nd->nd_cred, exp, p, NFSACCCHK_ALLOWOWNER,
		    NFSACCCHK_VPISLOCKED, NULL);
	    } else {
		nd->nd_repstat = nfsvno_accchk(vp, VREAD,
		    nd->nd_cred, exp, p, NFSACCCHK_ALLOWOWNER,
		    NFSACCCHK_VPISLOCKED, NULL);
		if (nd->nd_repstat)
		    nd->nd_repstat = nfsvno_accchk(vp, VEXEC,
			nd->nd_cred, exp, p, NFSACCCHK_ALLOWOWNER,
			NFSACCCHK_VPISLOCKED, NULL);
	    }
	}

	/*
	 * We call nfsrv_lockctrl() even if nd_repstat set, so that the
	 * seqid# gets updated. nfsrv_lockctrl() will return the value
	 * of nd_repstat, if it gets that far.
	 */
	nd->nd_repstat = nfsrv_lockctrl(vp, &stp, &lop, &cf, clientid, 
		&stateid, exp, nd, p);
	if (lop)
		FREE((caddr_t)lop, M_NFSDLOCK);
	if (stp)
		FREE((caddr_t)stp, M_NFSDSTATE);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
		*tl++ = txdr_unsigned(stateid.seqid);
		NFSBCOPY((caddr_t)stateid.other,(caddr_t)tl,NFSX_STATEIDOTHER);
	} else if (nd->nd_repstat == NFSERR_DENIED) {
		NFSM_BUILD(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
		txdr_hyper(cf.cl_first, tl);
		tl += 2;
		if (cf.cl_end == NFS64BITSSET)
			len = NFS64BITSSET;
		else
			len = cf.cl_end - cf.cl_first;
		txdr_hyper(len, tl);
		tl += 2;
		if (cf.cl_flags == NFSLCK_WRITE)
			*tl++ = txdr_unsigned(NFSV4LOCKT_WRITE);
		else
			*tl++ = txdr_unsigned(NFSV4LOCKT_READ);
		*tl++ = stateid.other[0];
		*tl = stateid.other[1];
		(void) nfsm_strtom(nd, cf.cl_owner, cf.cl_ownerlen);
	}
	vput(vp);
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	if (stp)
		free((caddr_t)stp, M_NFSDSTATE);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 lock test service
 */
APPLESTATIC int
nfsrvd_lockt(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int i;
	struct nfsstate *stp = NULL;
	struct nfslock lo, *lop = &lo;
	struct nfslockconflict cf;
	int error = 0;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;
	u_int64_t len;

	NFSM_DISSECT(tl, u_int32_t *, 8 * NFSX_UNSIGNED);
	i = fxdr_unsigned(int, *(tl + 7));
	if (i <= 0 || i > NFSV4_OPAQUELIMIT) {
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	}
	MALLOC(stp, struct nfsstate *, sizeof (struct nfsstate) + i,
	    M_NFSDSTATE, M_WAITOK);
	stp->ls_ownerlen = i;
	stp->ls_op = NULL;
	stp->ls_flags = NFSLCK_TEST;
	stp->ls_uid = nd->nd_cred->cr_uid;
	i = fxdr_unsigned(int, *tl++);
	switch (i) {
	case NFSV4LOCKT_READW:
		stp->ls_flags |= NFSLCK_BLOCKING;
	case NFSV4LOCKT_READ:
		lo.lo_flags = NFSLCK_READ;
		break;
	case NFSV4LOCKT_WRITEW:
		stp->ls_flags |= NFSLCK_BLOCKING;
	case NFSV4LOCKT_WRITE:
		lo.lo_flags = NFSLCK_WRITE;
		break;
	default:
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	};
	lo.lo_first = fxdr_hyper(tl);
	tl += 2;
	len = fxdr_hyper(tl);
	if (len == NFS64BITSSET) {
		lo.lo_end = NFS64BITSSET;
	} else {
		lo.lo_end = lo.lo_first + len;
		if (lo.lo_end <= lo.lo_first)
			nd->nd_repstat = NFSERR_INVAL;
	}
	tl += 2;
	clientid.lval[0] = *tl++;
	clientid.lval[1] = *tl;
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	error = nfsrv_mtostr(nd, stp->ls_owner, stp->ls_ownerlen);
	if (error)
		goto nfsmout;
	if (!nd->nd_repstat && vnode_vtype(vp) != VREG) {
	    if (vnode_vtype(vp) == VDIR)
		nd->nd_repstat = NFSERR_ISDIR;
	    else
		nd->nd_repstat = NFSERR_INVAL;
	}
	if (!nd->nd_repstat)
	  nd->nd_repstat = nfsrv_lockctrl(vp, &stp, &lop, &cf, clientid,
	    &stateid, exp, nd, p);
	if (stp)
		FREE((caddr_t)stp, M_NFSDSTATE);
	if (nd->nd_repstat) {
	    if (nd->nd_repstat == NFSERR_DENIED) {
		NFSM_BUILD(tl, u_int32_t *, 7 * NFSX_UNSIGNED);
		txdr_hyper(cf.cl_first, tl);
		tl += 2;
		if (cf.cl_end == NFS64BITSSET)
			len = NFS64BITSSET;
		else
			len = cf.cl_end - cf.cl_first;
		txdr_hyper(len, tl);
		tl += 2;
		if (cf.cl_flags == NFSLCK_WRITE)
			*tl++ = txdr_unsigned(NFSV4LOCKT_WRITE);
		else
			*tl++ = txdr_unsigned(NFSV4LOCKT_READ);
		*tl++ = stp->ls_stateid.other[0];
		*tl = stp->ls_stateid.other[1];
		(void) nfsm_strtom(nd, cf.cl_owner, cf.cl_ownerlen);
	    }
	}
	vput(vp);
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	if (stp)
		free((caddr_t)stp, M_NFSDSTATE);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 unlock service
 */
APPLESTATIC int
nfsrvd_locku(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int i;
	struct nfsstate *stp;
	struct nfslock *lop;
	int error = 0;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;
	u_int64_t len;

	NFSM_DISSECT(tl, u_int32_t *, 6 * NFSX_UNSIGNED + NFSX_STATEID);
	MALLOC(stp, struct nfsstate *, sizeof (struct nfsstate),
	    M_NFSDSTATE, M_WAITOK);
	MALLOC(lop, struct nfslock *, sizeof (struct nfslock),
	    M_NFSDLOCK, M_WAITOK);
	stp->ls_flags = NFSLCK_UNLOCK;
	lop->lo_flags = NFSLCK_UNLOCK;
	stp->ls_op = nd->nd_rp;
	i = fxdr_unsigned(int, *tl++);
	switch (i) {
	case NFSV4LOCKT_READW:
		stp->ls_flags |= NFSLCK_BLOCKING;
	case NFSV4LOCKT_READ:
		break;
	case NFSV4LOCKT_WRITEW:
		stp->ls_flags |= NFSLCK_BLOCKING;
	case NFSV4LOCKT_WRITE:
		break;
	default:
		nd->nd_repstat = NFSERR_BADXDR;
		free(stp, M_NFSDSTATE);
		free(lop, M_NFSDLOCK);
		goto nfsmout;
	};
	stp->ls_ownerlen = 0;
	stp->ls_uid = nd->nd_cred->cr_uid;
	stp->ls_seq = fxdr_unsigned(int, *tl++);
	stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
	NFSBCOPY((caddr_t)tl, (caddr_t)stp->ls_stateid.other,
	    NFSX_STATEIDOTHER);
	tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
	lop->lo_first = fxdr_hyper(tl);
	tl += 2;
	len = fxdr_hyper(tl);
	if (len == NFS64BITSSET) {
		lop->lo_end = NFS64BITSSET;
	} else {
		lop->lo_end = lop->lo_first + len;
		if (lop->lo_end <= lop->lo_first)
			nd->nd_repstat = NFSERR_INVAL;
	}
	clientid.lval[0] = stp->ls_stateid.other[0];
	clientid.lval[1] = stp->ls_stateid.other[1];
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	if (!nd->nd_repstat && vnode_vtype(vp) != VREG) {
	    if (vnode_vtype(vp) == VDIR)
		nd->nd_repstat = NFSERR_ISDIR;
	    else
		nd->nd_repstat = NFSERR_INVAL;
	}
	/*
	 * Call nfsrv_lockctrl() even if nd_repstat is set, so that the
	 * seqid# gets incremented. nfsrv_lockctrl() will return the
	 * value of nd_repstat, if it gets that far.
	 */
	nd->nd_repstat = nfsrv_lockctrl(vp, &stp, &lop, NULL, clientid,
	    &stateid, exp, nd, p);
	if (stp)
		FREE((caddr_t)stp, M_NFSDSTATE);
	if (lop)
		free((caddr_t)lop, M_NFSDLOCK);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
		*tl++ = txdr_unsigned(stateid.seqid);
		NFSBCOPY((caddr_t)stateid.other,(caddr_t)tl,NFSX_STATEIDOTHER);
	}
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 open service
 */
APPLESTATIC int
nfsrvd_open(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, vnode_t *vpp, __unused fhandle_t *fhp, NFSPROC_T *p,
    struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int i;
	struct nfsstate *stp = NULL;
	int error = 0, create, claim, exclusive_flag = 0;
	u_int32_t rflags = NFSV4OPEN_LOCKTYPEPOSIX, acemask;
	int how = NFSCREATE_UNCHECKED;
	int32_t cverf[2], tverf[2] = { 0, 0 };
	vnode_t vp = NULL, dirp = NULL;
	struct nfsvattr nva, dirfor, diraft;
	struct nameidata named;
	nfsv4stateid_t stateid, delegstateid;
	nfsattrbit_t attrbits;
	nfsquad_t clientid;
	char *bufp = NULL;
	u_long *hashp;
	NFSACL_T *aclp = NULL;

#ifdef NFS4_ACL_EXTATTR_NAME
	aclp = acl_alloc(M_WAITOK);
	aclp->acl_cnt = 0;
#endif
	NFSZERO_ATTRBIT(&attrbits);
	named.ni_startdir = NULL;
	named.ni_cnd.cn_nameiop = 0;
	NFSM_DISSECT(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
	i = fxdr_unsigned(int, *(tl + 5));
	if (i <= 0 || i > NFSV4_OPAQUELIMIT) {
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	}
	MALLOC(stp, struct nfsstate *, sizeof (struct nfsstate) + i,
	    M_NFSDSTATE, M_WAITOK);
	stp->ls_ownerlen = i;
	stp->ls_op = nd->nd_rp;
	stp->ls_flags = NFSLCK_OPEN;
	stp->ls_uid = nd->nd_cred->cr_uid;
	stp->ls_seq = fxdr_unsigned(u_int32_t, *tl++);
	i = fxdr_unsigned(int, *tl++);
	switch (i) {
	case NFSV4OPEN_ACCESSREAD:
		stp->ls_flags |= NFSLCK_READACCESS;
		break;
	case NFSV4OPEN_ACCESSWRITE:
		stp->ls_flags |= NFSLCK_WRITEACCESS;
		break;
	case NFSV4OPEN_ACCESSBOTH:
		stp->ls_flags |= (NFSLCK_READACCESS | NFSLCK_WRITEACCESS);
		break;
	default:
		nd->nd_repstat = NFSERR_INVAL;
	};
	i = fxdr_unsigned(int, *tl++);
	switch (i) {
	case NFSV4OPEN_DENYNONE:
		break;
	case NFSV4OPEN_DENYREAD:
		stp->ls_flags |= NFSLCK_READDENY;
		break;
	case NFSV4OPEN_DENYWRITE:
		stp->ls_flags |= NFSLCK_WRITEDENY;
		break;
	case NFSV4OPEN_DENYBOTH:
		stp->ls_flags |= (NFSLCK_READDENY | NFSLCK_WRITEDENY);
		break;
	default:
		nd->nd_repstat = NFSERR_INVAL;
	};
	clientid.lval[0] = *tl++;
	clientid.lval[1] = *tl;
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	error = nfsrv_mtostr(nd, stp->ls_owner, stp->ls_ownerlen);
	if (error)
		goto nfsmout;
	NFSVNO_ATTRINIT(&nva);
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	create = fxdr_unsigned(int, *tl);
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_getattr(dp, &dirfor, nd->nd_cred, p, 0);
	if (create == NFSV4OPEN_CREATE) {
		nva.na_type = VREG;
		nva.na_mode = 0;
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		how = fxdr_unsigned(int, *tl);
		switch (how) {
		case NFSCREATE_UNCHECKED:
		case NFSCREATE_GUARDED:
			error = nfsv4_sattr(nd, &nva, &attrbits, aclp, p);
			if (error)
				goto nfsmout;
			/*
			 * If the na_gid being set is the same as that of
			 * the directory it is going in, clear it, since
			 * that is what will be set by default. This allows
			 * a user that isn't in that group to do the create.
			 */
			if (!nd->nd_repstat && NFSVNO_ISSETGID(&nva) &&
			    nva.na_gid == dirfor.na_gid)
				NFSVNO_UNSET(&nva, gid);
			if (!nd->nd_repstat)
				nd->nd_repstat = nfsrv_checkuidgid(nd, &nva);
			break;
		case NFSCREATE_EXCLUSIVE:
			NFSM_DISSECT(tl, u_int32_t *, NFSX_VERF);
			cverf[0] = *tl++;
			cverf[1] = *tl;
			break;
		default:
			nd->nd_repstat = NFSERR_BADXDR;
			goto nfsmout;
		};
	} else if (create != NFSV4OPEN_NOCREATE) {
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	}

	/*
	 * Now, handle the claim, which usually includes looking up a
	 * name in the directory referenced by dp. The exception is
	 * NFSV4OPEN_CLAIMPREVIOUS.
	 */
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	claim = fxdr_unsigned(int, *tl);
	if (claim == NFSV4OPEN_CLAIMDELEGATECUR) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
		stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
		NFSBCOPY((caddr_t)tl,(caddr_t)stateid.other,NFSX_STATEIDOTHER);
		stp->ls_flags |= NFSLCK_DELEGCUR;
	} else if (claim == NFSV4OPEN_CLAIMDELEGATEPREV) {
		stp->ls_flags |= NFSLCK_DELEGPREV;
	}
	if (claim == NFSV4OPEN_CLAIMNULL || claim == NFSV4OPEN_CLAIMDELEGATECUR
	    || claim == NFSV4OPEN_CLAIMDELEGATEPREV) {
		if (!nd->nd_repstat && create == NFSV4OPEN_CREATE &&
		    claim != NFSV4OPEN_CLAIMNULL)
			nd->nd_repstat = NFSERR_INVAL;
		if (nd->nd_repstat) {
			nd->nd_repstat = nfsrv_opencheck(clientid,
			    &stateid, stp, NULL, nd, p, nd->nd_repstat);
			goto nfsmout;
		}
		if (create == NFSV4OPEN_CREATE)
		    NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, CREATE,
			LOCKPARENT | LOCKLEAF | SAVESTART);
		else
		    NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, LOOKUP,
			LOCKLEAF | SAVESTART);
		nfsvno_setpathbuf(&named, &bufp, &hashp);
		error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
		if (error) {
			vrele(dp);
#ifdef NFS4_ACL_EXTATTR_NAME
			acl_free(aclp);
#endif
			FREE((caddr_t)stp, M_NFSDSTATE);
			nfsvno_relpathbuf(&named);
			NFSEXITCODE2(error, nd);
			return (error);
		}
		if (!nd->nd_repstat) {
			nd->nd_repstat = nfsvno_namei(nd, &named, dp, 0, exp,
			    p, &dirp);
		} else {
			vrele(dp);
			nfsvno_relpathbuf(&named);
		}
		if (create == NFSV4OPEN_CREATE) {
		    switch (how) {
		    case NFSCREATE_UNCHECKED:
			if (named.ni_vp) {
				/*
				 * Clear the setable attribute bits, except
				 * for Size, if it is being truncated.
				 */
				NFSZERO_ATTRBIT(&attrbits);
				if (NFSVNO_ISSETSIZE(&nva))
					NFSSETBIT_ATTRBIT(&attrbits,
					    NFSATTRBIT_SIZE);
			}
			break;
		    case NFSCREATE_GUARDED:
			if (named.ni_vp && !nd->nd_repstat)
				nd->nd_repstat = EEXIST;
			break;
		    case NFSCREATE_EXCLUSIVE:
			exclusive_flag = 1;
			if (!named.ni_vp)
				nva.na_mode = 0;
		    };
		}
		nfsvno_open(nd, &named, clientid, &stateid, stp,
		    &exclusive_flag, &nva, cverf, create, aclp, &attrbits,
		    nd->nd_cred, p, exp, &vp);
	} else if (claim == NFSV4OPEN_CLAIMPREVIOUS) {
		NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
		i = fxdr_unsigned(int, *tl);
		switch (i) {
		case NFSV4OPEN_DELEGATEREAD:
			stp->ls_flags |= NFSLCK_DELEGREAD;
			break;
		case NFSV4OPEN_DELEGATEWRITE:
			stp->ls_flags |= NFSLCK_DELEGWRITE;
		case NFSV4OPEN_DELEGATENONE:
			break;
		default:
			nd->nd_repstat = NFSERR_BADXDR;
			goto nfsmout;
		};
		stp->ls_flags |= NFSLCK_RECLAIM;
		vp = dp;
		NFSVOPLOCK(vp, LK_EXCLUSIVE | LK_RETRY);
		if ((vp->v_iflag & VI_DOOMED) == 0)
			nd->nd_repstat = nfsrv_opencheck(clientid, &stateid,
			    stp, vp, nd, p, nd->nd_repstat);
		else
			nd->nd_repstat = NFSERR_PERM;
	} else {
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	}

	/*
	 * Do basic access checking.
	 */
	if (!nd->nd_repstat && vnode_vtype(vp) != VREG) {
		/*
		 * The IETF working group decided that this is the correct
		 * error return for all non-regular files.
		 */
		nd->nd_repstat = NFSERR_SYMLINK;
	}
	if (!nd->nd_repstat && (stp->ls_flags & NFSLCK_WRITEACCESS))
	    nd->nd_repstat = nfsvno_accchk(vp, VWRITE, nd->nd_cred,
	        exp, p, NFSACCCHK_ALLOWOWNER, NFSACCCHK_VPISLOCKED, NULL);
	if (!nd->nd_repstat && (stp->ls_flags & NFSLCK_READACCESS)) {
	    nd->nd_repstat = nfsvno_accchk(vp, VREAD, nd->nd_cred,
	        exp, p, NFSACCCHK_ALLOWOWNER, NFSACCCHK_VPISLOCKED, NULL);
	    if (nd->nd_repstat)
		nd->nd_repstat = nfsvno_accchk(vp, VEXEC,
		    nd->nd_cred, exp, p, NFSACCCHK_ALLOWOWNER,
		    NFSACCCHK_VPISLOCKED, NULL);
	}

	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
		if (!nd->nd_repstat) {
			tverf[0] = nva.na_atime.tv_sec;
			tverf[1] = nva.na_atime.tv_nsec;
		}
	}
	if (!nd->nd_repstat && exclusive_flag && (cverf[0] != tverf[0] ||
	    cverf[1] != tverf[1]))
		nd->nd_repstat = EEXIST;
	/*
	 * Do the open locking/delegation stuff.
	 */
	if (!nd->nd_repstat)
	    nd->nd_repstat = nfsrv_openctrl(nd, vp, &stp, clientid, &stateid,
		&delegstateid, &rflags, exp, p, nva.na_filerev);

	/*
	 * vp must be unlocked before the call to nfsvno_getattr(dirp,...)
	 * below, to avoid a deadlock with the lookup in nfsvno_namei() above.
	 * (ie: Leave the NFSVOPUNLOCK() about here.)
	 */
	if (vp)
		NFSVOPUNLOCK(vp, 0);
	if (stp)
		FREE((caddr_t)stp, M_NFSDSTATE);
	if (!nd->nd_repstat && dirp)
		nd->nd_repstat = nfsvno_getattr(dirp, &diraft, nd->nd_cred, p,
		    0);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID + 6 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(stateid.seqid);
		NFSBCOPY((caddr_t)stateid.other,(caddr_t)tl,NFSX_STATEIDOTHER);
		tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
		if (claim == NFSV4OPEN_CLAIMPREVIOUS) {
			*tl++ = newnfs_true;
			*tl++ = 0;
			*tl++ = 0;
			*tl++ = 0;
			*tl++ = 0;
		} else {
			*tl++ = newnfs_false;	/* Since dirp is not locked */
			txdr_hyper(dirfor.na_filerev, tl);
			tl += 2;
			txdr_hyper(diraft.na_filerev, tl);
			tl += 2;
		}
		*tl = txdr_unsigned(rflags & NFSV4OPEN_RFLAGS);
		(void) nfsrv_putattrbit(nd, &attrbits);
		NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
		if (rflags & NFSV4OPEN_READDELEGATE)
			*tl = txdr_unsigned(NFSV4OPEN_DELEGATEREAD);
		else if (rflags & NFSV4OPEN_WRITEDELEGATE)
			*tl = txdr_unsigned(NFSV4OPEN_DELEGATEWRITE);
		else
			*tl = txdr_unsigned(NFSV4OPEN_DELEGATENONE);
		if (rflags & (NFSV4OPEN_READDELEGATE|NFSV4OPEN_WRITEDELEGATE)) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID+NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(delegstateid.seqid);
			NFSBCOPY((caddr_t)delegstateid.other, (caddr_t)tl,
			    NFSX_STATEIDOTHER);
			tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
			if (rflags & NFSV4OPEN_RECALL)
				*tl = newnfs_true;
			else
				*tl = newnfs_false;
			if (rflags & NFSV4OPEN_WRITEDELEGATE) {
				NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
				*tl++ = txdr_unsigned(NFSV4OPEN_LIMITSIZE);
				txdr_hyper(nva.na_size, tl);
			}
			NFSM_BUILD(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(NFSV4ACE_ALLOWEDTYPE);
			*tl++ = txdr_unsigned(0x0);
			acemask = NFSV4ACE_ALLFILESMASK;
			if (nva.na_mode & S_IRUSR)
			    acemask |= NFSV4ACE_READMASK;
			if (nva.na_mode & S_IWUSR)
			    acemask |= NFSV4ACE_WRITEMASK;
			if (nva.na_mode & S_IXUSR)
			    acemask |= NFSV4ACE_EXECUTEMASK;
			*tl = txdr_unsigned(acemask);
			(void) nfsm_strtom(nd, "OWNER@", 6);
		}
		*vpp = vp;
	} else if (vp) {
		vrele(vp);
	}
	if (dirp)
		vrele(dirp);
#ifdef NFS4_ACL_EXTATTR_NAME
	acl_free(aclp);
#endif
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vrele(dp);
#ifdef NFS4_ACL_EXTATTR_NAME
	acl_free(aclp);
#endif
	if (stp)
		FREE((caddr_t)stp, M_NFSDSTATE);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 close service
 */
APPLESTATIC int
nfsrvd_close(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	struct nfsstate st, *stp = &st;
	int error = 0;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED + NFSX_STATEID);
	stp->ls_seq = fxdr_unsigned(u_int32_t, *tl++);
	stp->ls_ownerlen = 0;
	stp->ls_op = nd->nd_rp;
	stp->ls_uid = nd->nd_cred->cr_uid;
	stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
	NFSBCOPY((caddr_t)tl, (caddr_t)stp->ls_stateid.other,
	    NFSX_STATEIDOTHER);
	stp->ls_flags = NFSLCK_CLOSE;
	clientid.lval[0] = stp->ls_stateid.other[0];
	clientid.lval[1] = stp->ls_stateid.other[1];
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	nd->nd_repstat = nfsrv_openupdate(vp, stp, clientid, &stateid, nd, p);
	vput(vp);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
		*tl++ = txdr_unsigned(stateid.seqid);
		NFSBCOPY((caddr_t)stateid.other,(caddr_t)tl,NFSX_STATEIDOTHER);
	}
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 delegpurge service
 */
APPLESTATIC int
nfsrvd_delegpurge(struct nfsrv_descript *nd, __unused int isdgram,
    __unused vnode_t vp, __unused NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int error = 0;
	nfsquad_t clientid;

	if (nfs_rootfhset == 0 || nfsd_checkrootexp(nd) != 0) {
		nd->nd_repstat = NFSERR_WRONGSEC;
		goto nfsmout;
	}
	NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	clientid.lval[0] = *tl++;
	clientid.lval[1] = *tl;
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	nd->nd_repstat = nfsrv_delegupdate(clientid, NULL, NULL,
	    NFSV4OP_DELEGPURGE, nd->nd_cred, p);
nfsmout:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 delegreturn service
 */
APPLESTATIC int
nfsrvd_delegreturn(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int error = 0;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID);
	stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
	NFSBCOPY((caddr_t)tl, (caddr_t)stateid.other, NFSX_STATEIDOTHER);
	clientid.lval[0] = stateid.other[0];
	clientid.lval[1] = stateid.other[1];
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	nd->nd_repstat = nfsrv_delegupdate(clientid, &stateid, vp,
	    NFSV4OP_DELEGRETURN, nd->nd_cred, p);
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 get file handle service
 */
APPLESTATIC int
nfsrvd_getfh(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	fhandle_t fh;

	nd->nd_repstat = nfsvno_getfh(vp, &fh, p);
	vput(vp);
	if (!nd->nd_repstat)
		(void) nfsm_fhtom(nd, (u_int8_t *)&fh, 0, 0);
	NFSEXITCODE2(0, nd);
	return (0);
}

/*
 * nfsv4 open confirm service
 */
APPLESTATIC int
nfsrvd_openconfirm(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	struct nfsstate st, *stp = &st;
	int error = 0;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID + NFSX_UNSIGNED);
	stp->ls_ownerlen = 0;
	stp->ls_op = nd->nd_rp;
	stp->ls_uid = nd->nd_cred->cr_uid;
	stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
	NFSBCOPY((caddr_t)tl, (caddr_t)stp->ls_stateid.other,
	    NFSX_STATEIDOTHER);
	tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
	stp->ls_seq = fxdr_unsigned(u_int32_t, *tl);
	stp->ls_flags = NFSLCK_CONFIRM;
	clientid.lval[0] = stp->ls_stateid.other[0];
	clientid.lval[1] = stp->ls_stateid.other[1];
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	nd->nd_repstat = nfsrv_openupdate(vp, stp, clientid, &stateid, nd, p);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
		*tl++ = txdr_unsigned(stateid.seqid);
		NFSBCOPY((caddr_t)stateid.other,(caddr_t)tl,NFSX_STATEIDOTHER);
	}
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 open downgrade service
 */
APPLESTATIC int
nfsrvd_opendowngrade(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int i;
	struct nfsstate st, *stp = &st;
	int error = 0;
	nfsv4stateid_t stateid;
	nfsquad_t clientid;

	NFSM_DISSECT(tl, u_int32_t *, NFSX_STATEID + 3 * NFSX_UNSIGNED);
	stp->ls_ownerlen = 0;
	stp->ls_op = nd->nd_rp;
	stp->ls_uid = nd->nd_cred->cr_uid;
	stp->ls_stateid.seqid = fxdr_unsigned(u_int32_t, *tl++);
	NFSBCOPY((caddr_t)tl, (caddr_t)stp->ls_stateid.other,
	    NFSX_STATEIDOTHER);
	tl += (NFSX_STATEIDOTHER / NFSX_UNSIGNED);
	stp->ls_seq = fxdr_unsigned(u_int32_t, *tl++);
	i = fxdr_unsigned(int, *tl++);
	switch (i) {
	case NFSV4OPEN_ACCESSREAD:
		stp->ls_flags = (NFSLCK_READACCESS | NFSLCK_DOWNGRADE);
		break;
	case NFSV4OPEN_ACCESSWRITE:
		stp->ls_flags = (NFSLCK_WRITEACCESS | NFSLCK_DOWNGRADE);
		break;
	case NFSV4OPEN_ACCESSBOTH:
		stp->ls_flags = (NFSLCK_READACCESS | NFSLCK_WRITEACCESS |
		    NFSLCK_DOWNGRADE);
		break;
	default:
		nd->nd_repstat = NFSERR_BADXDR;
	};
	i = fxdr_unsigned(int, *tl);
	switch (i) {
	case NFSV4OPEN_DENYNONE:
		break;
	case NFSV4OPEN_DENYREAD:
		stp->ls_flags |= NFSLCK_READDENY;
		break;
	case NFSV4OPEN_DENYWRITE:
		stp->ls_flags |= NFSLCK_WRITEDENY;
		break;
	case NFSV4OPEN_DENYBOTH:
		stp->ls_flags |= (NFSLCK_READDENY | NFSLCK_WRITEDENY);
		break;
	default:
		nd->nd_repstat = NFSERR_BADXDR;
	};

	clientid.lval[0] = stp->ls_stateid.other[0];
	clientid.lval[1] = stp->ls_stateid.other[1];
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsrv_openupdate(vp, stp, clientid, &stateid,
		    nd, p);
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, NFSX_STATEID);
		*tl++ = txdr_unsigned(stateid.seqid);
		NFSBCOPY((caddr_t)stateid.other,(caddr_t)tl,NFSX_STATEIDOTHER);
	}
nfsmout:
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 renew lease service
 */
APPLESTATIC int
nfsrvd_renew(struct nfsrv_descript *nd, __unused int isdgram,
    __unused vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int error = 0;
	nfsquad_t clientid;

	if (nfs_rootfhset == 0 || nfsd_checkrootexp(nd) != 0) {
		nd->nd_repstat = NFSERR_WRONGSEC;
		goto nfsmout;
	}
	NFSM_DISSECT(tl, u_int32_t *, NFSX_HYPER);
	clientid.lval[0] = *tl++;
	clientid.lval[1] = *tl;
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	nd->nd_repstat = nfsrv_getclient(clientid, (CLOPS_RENEWOP|CLOPS_RENEW),
	    NULL, (nfsquad_t)((u_quad_t)0), nd, p);
nfsmout:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 security info service
 */
APPLESTATIC int
nfsrvd_secinfo(struct nfsrv_descript *nd, int isdgram,
    vnode_t dp, NFSPROC_T *p, struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int len;
	struct nameidata named;
	vnode_t dirp = NULL, vp;
	struct nfsrvfh fh;
	struct nfsexstuff retnes;
	u_int32_t *sizp;
	int error = 0, savflag, i;
	char *bufp;
	u_long *hashp;

	/*
	 * All this just to get the export flags for the name.
	 */
	NFSNAMEICNDSET(&named.ni_cnd, nd->nd_cred, LOOKUP,
	    LOCKLEAF | SAVESTART);
	nfsvno_setpathbuf(&named, &bufp, &hashp);
	error = nfsrv_parsename(nd, bufp, hashp, &named.ni_pathlen);
	if (error) {
		vput(dp);
		nfsvno_relpathbuf(&named);
		goto out;
	}
	if (!nd->nd_repstat) {
		nd->nd_repstat = nfsvno_namei(nd, &named, dp, 1, exp, p, &dirp);
	} else {
		vput(dp);
		nfsvno_relpathbuf(&named);
	}
	if (dirp)
		vrele(dirp);
	if (nd->nd_repstat)
		goto out;
	vrele(named.ni_startdir);
	nfsvno_relpathbuf(&named);
	fh.nfsrvfh_len = NFSX_MYFH;
	vp = named.ni_vp;
	nd->nd_repstat = nfsvno_getfh(vp, (fhandle_t *)fh.nfsrvfh_data, p);
	vput(vp);
	savflag = nd->nd_flag;
	if (!nd->nd_repstat) {
		nfsd_fhtovp(nd, &fh, LK_SHARED, &vp, &retnes, NULL, 0, p);
		if (vp)
			vput(vp);
	}
	nd->nd_flag = savflag;
	if (nd->nd_repstat)
		goto out;

	/*
	 * Finally have the export flags for name, so we can create
	 * the security info.
	 */
	len = 0;
	NFSM_BUILD(sizp, u_int32_t *, NFSX_UNSIGNED);
	for (i = 0; i < retnes.nes_numsecflavor; i++) {
		if (retnes.nes_secflavors[i] == AUTH_SYS) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl = txdr_unsigned(RPCAUTH_UNIX);
			len++;
		} else if (retnes.nes_secflavors[i] == RPCSEC_GSS_KRB5) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(RPCAUTH_GSS);
			(void) nfsm_strtom(nd, nfsgss_mechlist[KERBV_MECH].str,
			    nfsgss_mechlist[KERBV_MECH].len);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(GSS_KERBV_QOP);
			*tl = txdr_unsigned(RPCAUTHGSS_SVCNONE);
			len++;
		} else if (retnes.nes_secflavors[i] == RPCSEC_GSS_KRB5I) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(RPCAUTH_GSS);
			(void) nfsm_strtom(nd, nfsgss_mechlist[KERBV_MECH].str,
			    nfsgss_mechlist[KERBV_MECH].len);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(GSS_KERBV_QOP);
			*tl = txdr_unsigned(RPCAUTHGSS_SVCINTEGRITY);
			len++;
		} else if (retnes.nes_secflavors[i] == RPCSEC_GSS_KRB5P) {
			NFSM_BUILD(tl, u_int32_t *, NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(RPCAUTH_GSS);
			(void) nfsm_strtom(nd, nfsgss_mechlist[KERBV_MECH].str,
			    nfsgss_mechlist[KERBV_MECH].len);
			NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(GSS_KERBV_QOP);
			*tl = txdr_unsigned(RPCAUTHGSS_SVCPRIVACY);
			len++;
		}
	}
	*sizp = txdr_unsigned(len);

out:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 set client id service
 */
APPLESTATIC int
nfsrvd_setclientid(struct nfsrv_descript *nd, __unused int isdgram,
    __unused vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int i;
	int error = 0, idlen;
	struct nfsclient *clp = NULL;
	struct sockaddr_in *rad;
	u_char *verf, *ucp, *ucp2, addrbuf[24];
	nfsquad_t clientid, confirm;

	if (nfs_rootfhset == 0 || nfsd_checkrootexp(nd) != 0) {
		nd->nd_repstat = NFSERR_WRONGSEC;
		goto out;
	}
	NFSM_DISSECT(tl, u_int32_t *, NFSX_VERF + NFSX_UNSIGNED);
	verf = (u_char *)tl;
	tl += (NFSX_VERF / NFSX_UNSIGNED);
	i = fxdr_unsigned(int, *tl);
	if (i > NFSV4_OPAQUELIMIT || i <= 0) {
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	}
	idlen = i;
	if (nd->nd_flag & ND_GSS)
		i += nd->nd_princlen;
	MALLOC(clp, struct nfsclient *, sizeof (struct nfsclient) + i,
	    M_NFSDCLIENT, M_WAITOK);
	NFSBZERO((caddr_t)clp, sizeof (struct nfsclient) + i);
	NFSINITSOCKMUTEX(&clp->lc_req.nr_mtx);
	NFSSOCKADDRALLOC(clp->lc_req.nr_nam);
	NFSSOCKADDRSIZE(clp->lc_req.nr_nam, sizeof (struct sockaddr_in));
	clp->lc_req.nr_cred = NULL;
	NFSBCOPY(verf, clp->lc_verf, NFSX_VERF);
	clp->lc_idlen = idlen;
	error = nfsrv_mtostr(nd, clp->lc_id, idlen);
	if (error)
		goto nfsmout;
	if (nd->nd_flag & ND_GSS) {
		clp->lc_flags = LCL_GSS;
		if (nd->nd_flag & ND_GSSINTEGRITY)
			clp->lc_flags |= LCL_GSSINTEGRITY;
		else if (nd->nd_flag & ND_GSSPRIVACY)
			clp->lc_flags |= LCL_GSSPRIVACY;
	} else {
		clp->lc_flags = 0;
	}
	if ((nd->nd_flag & ND_GSS) && nd->nd_princlen > 0) {
		clp->lc_flags |= LCL_NAME;
		clp->lc_namelen = nd->nd_princlen;
		clp->lc_name = &clp->lc_id[idlen];
		NFSBCOPY(nd->nd_principal, clp->lc_name, clp->lc_namelen);
	} else {
		clp->lc_uid = nd->nd_cred->cr_uid;
		clp->lc_gid = nd->nd_cred->cr_gid;
	}
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	clp->lc_program = fxdr_unsigned(u_int32_t, *tl);
	error = nfsrv_getclientipaddr(nd, clp);
	if (error)
		goto nfsmout;
	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	clp->lc_callback = fxdr_unsigned(u_int32_t, *tl);

	/*
	 * nfsrv_setclient() does the actual work of adding it to the
	 * client list. If there is no error, the structure has been
	 * linked into the client list and clp should no longer be used
	 * here. When an error is returned, it has not been linked in,
	 * so it should be free'd.
	 */
	nd->nd_repstat = nfsrv_setclient(nd, &clp, &clientid, &confirm, p);
	if (nd->nd_repstat == NFSERR_CLIDINUSE) {
		if (clp->lc_flags & LCL_TCPCALLBACK)
			(void) nfsm_strtom(nd, "tcp", 3);
		else 
			(void) nfsm_strtom(nd, "udp", 3);
		rad = NFSSOCKADDR(clp->lc_req.nr_nam, struct sockaddr_in *);
		ucp = (u_char *)&rad->sin_addr.s_addr;
		ucp2 = (u_char *)&rad->sin_port;
		snprintf(addrbuf, sizeof(addrbuf), "%d.%d.%d.%d.%d.%d",
		    ucp[0] & 0xff, ucp[1] & 0xff, ucp[2] & 0xff, ucp[3] & 0xff,
		    ucp2[0] & 0xff, ucp2[1] & 0xff);
		(void) nfsm_strtom(nd, addrbuf, strlen(addrbuf));
	}
	if (clp) {
		NFSSOCKADDRFREE(clp->lc_req.nr_nam);
		NFSFREEMUTEX(&clp->lc_req.nr_mtx);
		free((caddr_t)clp, M_NFSDCLIENT);
	}
	if (!nd->nd_repstat) {
		NFSM_BUILD(tl, u_int32_t *, 2 * NFSX_HYPER);
		*tl++ = clientid.lval[0];
		*tl++ = clientid.lval[1];
		*tl++ = confirm.lval[0];
		*tl = confirm.lval[1];
	}

out:
	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	if (clp) {
		NFSSOCKADDRFREE(clp->lc_req.nr_nam);
		NFSFREEMUTEX(&clp->lc_req.nr_mtx);
		free((caddr_t)clp, M_NFSDCLIENT);
	}
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 set client id confirm service
 */
APPLESTATIC int
nfsrvd_setclientidcfrm(struct nfsrv_descript *nd,
    __unused int isdgram, __unused vnode_t vp, NFSPROC_T *p,
    __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int error = 0;
	nfsquad_t clientid, confirm;

	if (nfs_rootfhset == 0 || nfsd_checkrootexp(nd) != 0) {
		nd->nd_repstat = NFSERR_WRONGSEC;
		goto nfsmout;
	}
	NFSM_DISSECT(tl, u_int32_t *, 2 * NFSX_HYPER);
	clientid.lval[0] = *tl++;
	clientid.lval[1] = *tl++;
	confirm.lval[0] = *tl++;
	confirm.lval[1] = *tl;

	/*
	 * nfsrv_getclient() searches the client list for a match and
	 * returns the appropriate NFSERR status.
	 */
	nd->nd_repstat = nfsrv_getclient(clientid, (CLOPS_CONFIRM|CLOPS_RENEW),
	    NULL, confirm, nd, p);
nfsmout:
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 verify service
 */
APPLESTATIC int
nfsrvd_verify(struct nfsrv_descript *nd, int isdgram,
    vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	int error = 0, ret, fhsize = NFSX_MYFH;
	struct nfsvattr nva;
	struct statfs sf;
	struct nfsfsinfo fs;
	fhandle_t fh;

	nd->nd_repstat = nfsvno_getattr(vp, &nva, nd->nd_cred, p, 1);
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_statfs(vp, &sf);
	if (!nd->nd_repstat)
		nd->nd_repstat = nfsvno_getfh(vp, &fh, p);
	if (!nd->nd_repstat) {
		nfsvno_getfs(&fs, isdgram);
		error = nfsv4_loadattr(nd, vp, &nva, NULL, &fh, fhsize, NULL,
		    &sf, NULL, &fs, NULL, 1, &ret, NULL, NULL, p, nd->nd_cred);
		if (!error) {
			if (nd->nd_procnum == NFSV4OP_NVERIFY) {
				if (ret == 0)
					nd->nd_repstat = NFSERR_SAME;
				else if (ret != NFSERR_NOTSAME)
					nd->nd_repstat = ret;
			} else if (ret)
				nd->nd_repstat = ret;
		}
	}
	vput(vp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfs openattr rpc
 */
APPLESTATIC int
nfsrvd_openattr(struct nfsrv_descript *nd, __unused int isdgram,
    vnode_t dp, __unused vnode_t *vpp, __unused fhandle_t *fhp,
    __unused NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	int error = 0, createdir;

	NFSM_DISSECT(tl, u_int32_t *, NFSX_UNSIGNED);
	createdir = fxdr_unsigned(int, *tl);
	nd->nd_repstat = NFSERR_NOTSUPP;
nfsmout:
	vrele(dp);
	NFSEXITCODE2(error, nd);
	return (error);
}

/*
 * nfsv4 release lock owner service
 */
APPLESTATIC int
nfsrvd_releaselckown(struct nfsrv_descript *nd, __unused int isdgram,
    __unused vnode_t vp, NFSPROC_T *p, __unused struct nfsexstuff *exp)
{
	u_int32_t *tl;
	struct nfsstate *stp = NULL;
	int error = 0, len;
	nfsquad_t clientid;

	if (nfs_rootfhset == 0 || nfsd_checkrootexp(nd) != 0) {
		nd->nd_repstat = NFSERR_WRONGSEC;
		goto nfsmout;
	}
	NFSM_DISSECT(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
	len = fxdr_unsigned(int, *(tl + 2));
	if (len <= 0 || len > NFSV4_OPAQUELIMIT) {
		nd->nd_repstat = NFSERR_BADXDR;
		goto nfsmout;
	}
	MALLOC(stp, struct nfsstate *, sizeof (struct nfsstate) + len,
	    M_NFSDSTATE, M_WAITOK);
	stp->ls_ownerlen = len;
	stp->ls_op = NULL;
	stp->ls_flags = NFSLCK_RELEASE;
	stp->ls_uid = nd->nd_cred->cr_uid;
	clientid.lval[0] = *tl++;
	clientid.lval[1] = *tl;
	if (nd->nd_flag & ND_IMPLIEDCLID) {
		if (nd->nd_clientid.qval != clientid.qval)
			printf("EEK! multiple clids\n");
	} else {
		nd->nd_flag |= ND_IMPLIEDCLID;
		nd->nd_clientid.qval = clientid.qval;
	}
	error = nfsrv_mtostr(nd, stp->ls_owner, len);
	if (error)
		goto nfsmout;
	nd->nd_repstat = nfsrv_releaselckown(stp, clientid, p);
	FREE((caddr_t)stp, M_NFSDSTATE);

	NFSEXITCODE2(0, nd);
	return (0);
nfsmout:
	if (stp)
		free((caddr_t)stp, M_NFSDSTATE);
	NFSEXITCODE2(error, nd);
	return (error);
}
