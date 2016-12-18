/*	$NetBSD: nfs_clnode.c,v 1.1.1.1 2013/09/30 07:19:20 dholland Exp $	*/
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
 *	from nfs_node.c	8.6 (Berkeley) 5/22/95
 */

#include <sys/cdefs.h>
/* __FBSDID("FreeBSD: head/sys/fs/nfsclient/nfs_clnode.c 248084 2013-03-09 02:32:23Z attilio "); */
__RCSID("$NetBSD: nfs_clnode.c,v 1.1.1.1 2013/09/30 07:19:20 dholland Exp $");

#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/vnode.h>

#include <vm/uma.h>

#include <fs/nfs/nfsport.h>
#include <fs/nfsclient/nfsnode.h>
#include <fs/nfsclient/nfsmount.h>
#include <fs/nfsclient/nfs.h>
#include <fs/nfsclient/nfs_kdtrace.h>

#include <nfs/nfs_lock.h>

extern struct vop_vector newnfs_vnodeops;
extern struct buf_ops buf_ops_newnfs;
MALLOC_DECLARE(M_NEWNFSREQ);

uma_zone_t newnfsnode_zone;

static void	nfs_freesillyrename(void *arg, __unused int pending);

void
ncl_nhinit(void)
{

	newnfsnode_zone = uma_zcreate("NCLNODE", sizeof(struct nfsnode), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}

void
ncl_nhuninit(void)
{
	uma_zdestroy(newnfsnode_zone);
}

/*
 * ONLY USED FOR THE ROOT DIRECTORY. nfscl_nget() does the rest. If this
 * function is going to be used to get Regular Files, code must be added
 * to fill in the "struct nfsv4node".
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int
ncl_nget(struct mount *mntp, u_int8_t *fhp, int fhsize, struct nfsnode **npp,
    int lkflags)
{
	struct thread *td = curthread;	/* XXX */
	struct nfsnode *np;
	struct vnode *vp;
	struct vnode *nvp;
	int error;
	u_int hash;
	struct nfsmount *nmp;
	struct nfsfh *nfhp;

	nmp = VFSTONFS(mntp);
	*npp = NULL;

	hash = fnv_32_buf(fhp, fhsize, FNV1_32_INIT);

	MALLOC(nfhp, struct nfsfh *, sizeof (struct nfsfh) + fhsize,
	    M_NFSFH, M_WAITOK);
	bcopy(fhp, &nfhp->nfh_fh[0], fhsize);
	nfhp->nfh_len = fhsize;
	error = vfs_hash_get(mntp, hash, lkflags,
	    td, &nvp, newnfs_vncmpf, nfhp);
	FREE(nfhp, M_NFSFH);
	if (error)
		return (error);
	if (nvp != NULL) {
		*npp = VTONFS(nvp);
		return (0);
	}
	np = uma_zalloc(newnfsnode_zone, M_WAITOK | M_ZERO);

	error = getnewvnode("newnfs", mntp, &newnfs_vnodeops, &nvp);
	if (error) {
		uma_zfree(newnfsnode_zone, np);
		return (error);
	}
	vp = nvp;
	KASSERT(vp->v_bufobj.bo_bsize != 0, ("ncl_nget: bo_bsize == 0"));
	vp->v_bufobj.bo_ops = &buf_ops_newnfs;
	vp->v_data = np;
	np->n_vnode = vp;
	/* 
	 * Initialize the mutex even if the vnode is going to be a loser.
	 * This simplifies the logic in reclaim, which can then unconditionally
	 * destroy the mutex (in the case of the loser, or if hash_insert
	 * happened to return an error no special casing is needed).
	 */
	mtx_init(&np->n_mtx, "NEWNFSnode lock", NULL, MTX_DEF | MTX_DUPOK);
	/*
	 * NFS supports recursive and shared locking.
	 */
	lockmgr(vp->v_vnlock, LK_EXCLUSIVE | LK_NOWITNESS, NULL);
	VN_LOCK_AREC(vp);
	VN_LOCK_ASHARE(vp);
	/* 
	 * Are we getting the root? If so, make sure the vnode flags
	 * are correct 
	 */
	if ((fhsize == nmp->nm_fhsize) &&
	    !bcmp(fhp, nmp->nm_fh, fhsize)) {
		if (vp->v_type == VNON)
			vp->v_type = VDIR;
		vp->v_vflag |= VV_ROOT;
	}
	
	MALLOC(np->n_fhp, struct nfsfh *, sizeof (struct nfsfh) + fhsize,
	    M_NFSFH, M_WAITOK);
	bcopy(fhp, np->n_fhp->nfh_fh, fhsize);
	np->n_fhp->nfh_len = fhsize;
	error = insmntque(vp, mntp);
	if (error != 0) {
		*npp = NULL;
		FREE((caddr_t)np->n_fhp, M_NFSFH);
		mtx_destroy(&np->n_mtx);
		uma_zfree(newnfsnode_zone, np);
		return (error);
	}
	error = vfs_hash_insert(vp, hash, lkflags, 
	    td, &nvp, newnfs_vncmpf, np->n_fhp);
	if (error)
		return (error);
	if (nvp != NULL) {
		*npp = VTONFS(nvp);
		/* vfs_hash_insert() vput()'s the losing vnode */
		return (0);
	}
	*npp = np;

	return (0);
}

/*
 * Do the vrele(sp->s_dvp) as a separate task in order to avoid a
 * deadlock because of a LOR when vrele() locks the directory vnode.
 */
static void
nfs_freesillyrename(void *arg, __unused int pending)
{
	struct sillyrename *sp;

	sp = arg;
	vrele(sp->s_dvp);
	free(sp, M_NEWNFSREQ);
}

int
ncl_inactive(struct vop_inactive_args *ap)
{
	struct nfsnode *np;
	struct sillyrename *sp;
	struct vnode *vp = ap->a_vp;
	boolean_t retv;

	np = VTONFS(vp);

	if (NFS_ISV4(vp) && vp->v_type == VREG) {
		/*
		 * Since mmap()'d files do I/O after VOP_CLOSE(), the NFSv4
		 * Close operations are delayed until now. Any dirty
		 * buffers/pages must be flushed before the close, so that the
		 * stateid is available for the writes.
		 */
		if (vp->v_object != NULL) {
			VM_OBJECT_WLOCK(vp->v_object);
			retv = vm_object_page_clean(vp->v_object, 0, 0,
			    OBJPC_SYNC);
			VM_OBJECT_WUNLOCK(vp->v_object);
		} else
			retv = TRUE;
		if (retv == TRUE) {
			(void)ncl_flush(vp, MNT_WAIT, NULL, ap->a_td, 1, 0);
			(void)nfsrpc_close(vp, 1, ap->a_td);
		}
	}

	mtx_lock(&np->n_mtx);
	if (vp->v_type != VDIR) {
		sp = np->n_sillyrename;
		np->n_sillyrename = NULL;
	} else
		sp = NULL;
	if (sp) {
		mtx_unlock(&np->n_mtx);
		(void) ncl_vinvalbuf(vp, 0, ap->a_td, 1);
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		ncl_removeit(sp, vp);
		crfree(sp->s_cred);
		TASK_INIT(&sp->s_task, 0, nfs_freesillyrename, sp);
		taskqueue_enqueue(taskqueue_thread, &sp->s_task);
		mtx_lock(&np->n_mtx);
	}
	np->n_flag &= NMODIFIED;
	mtx_unlock(&np->n_mtx);
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
int
ncl_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsdmap *dp, *dp2;

	/*
	 * If the NLM is running, give it a chance to abort pending
	 * locks.
	 */
	if (nfs_reclaim_p != NULL)
		nfs_reclaim_p(ap);

	/*
	 * Destroy the vm object and flush associated pages.
	 */
	vnode_destroy_vobject(vp);

	if (NFS_ISV4(vp) && vp->v_type == VREG)
		/*
		 * We can now safely close any remaining NFSv4 Opens for
		 * this file. Most opens will have already been closed by
		 * ncl_inactive(), but there are cases where it is not
		 * called, so we need to do it again here.
		 */
		(void) nfsrpc_close(vp, 1, ap->a_td);

	vfs_hash_remove(vp);

	/*
	 * Call nfscl_reclaimnode() to save attributes in the delegation,
	 * as required.
	 */
	if (vp->v_type == VREG)
		nfscl_reclaimnode(vp);

	/*
	 * Free up any directory cookie structures and
	 * large file handle structures that might be associated with
	 * this nfs node.
	 */
	if (vp->v_type == VDIR) {
		dp = LIST_FIRST(&np->n_cookies);
		while (dp) {
			dp2 = dp;
			dp = LIST_NEXT(dp, ndm_list);
			FREE((caddr_t)dp2, M_NFSDIROFF);
		}
	}
	if (np->n_writecred != NULL)
		crfree(np->n_writecred);
	FREE((caddr_t)np->n_fhp, M_NFSFH);
	if (np->n_v4 != NULL)
		FREE((caddr_t)np->n_v4, M_NFSV4NODE);
	mtx_destroy(&np->n_mtx);
	uma_zfree(newnfsnode_zone, vp->v_data);
	vp->v_data = NULL;
	return (0);
}

/*
 * Invalidate both the access and attribute caches for this vnode.
 */
void
ncl_invalcaches(struct vnode *vp)
{
	struct nfsnode *np = VTONFS(vp);
	int i;

	mtx_lock(&np->n_mtx);
	for (i = 0; i < NFS_ACCESSCACHESIZE; i++)
		np->n_accesscache[i].stamp = 0;
	KDTRACE_NFS_ACCESSCACHE_FLUSH_DONE(vp);
	np->n_attrstamp = 0;
	KDTRACE_NFS_ATTRCACHE_FLUSH_DONE(vp);
	mtx_unlock(&np->n_mtx);
}

