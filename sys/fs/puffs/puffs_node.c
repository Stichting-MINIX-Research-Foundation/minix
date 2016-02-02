/*	$NetBSD: puffs_node.c,v 1.36 2014/11/10 18:46:33 maxv Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program, the Ulla Tuominen Foundation
 * and the Finnish Cultural Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puffs_node.c,v 1.36 2014/11/10 18:46:33 maxv Exp $");

#include <sys/param.h>
#include <sys/hash.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

struct pool puffs_pnpool;
struct pool puffs_vapool;

/*
 * Grab a vnode, intialize all the puffs-dependent stuff.
 */
static int
puffs_getvnode1(struct mount *mp, puffs_cookie_t ck, enum vtype type,
	voff_t vsize, dev_t rdev, bool may_exist, struct vnode **vpp)
{
	struct puffs_mount *pmp;
	struct vnode *vp;
	struct puffs_node *pnode;
	int error;

	pmp = MPTOPUFFSMP(mp);

	if (type <= VNON || type >= VBAD) {
		puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EINVAL,
		    "bad node type", ck);
		return EPROTO;
	}
	if (vsize == VSIZENOTSET) {
		puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EINVAL,
		    "VSIZENOTSET is not a valid size", ck);
		return EPROTO;
	}

	for (;;) {
		error = vcache_get(mp, &ck, sizeof(ck), &vp);
		if (error)
			return error;
		mutex_enter(vp->v_interlock);
		pnode = VPTOPP(vp);
		if (pnode != NULL)
			break;
		mutex_exit(vp->v_interlock);
		vrele(vp);
	}
	mutex_enter(&pnode->pn_mtx);
	mutex_exit(vp->v_interlock);

	/*
	 * Release and error out if caller wants a fresh vnode.
	 */
	if (vp->v_type != VNON && ! may_exist) {
		mutex_exit(&pnode->pn_mtx);
		vrele(vp);
		return EEXIST;
	}

	*vpp = vp;

	/*
	 * If fully initialized were done.
	 */
	if (vp->v_type != VNON) {
		mutex_exit(&pnode->pn_mtx);
		return 0;
	}

	/*
	 * Set type and finalize the initialisation.
	 */
	vp->v_type = type;
	if (type == VCHR || type == VBLK) {
		vp->v_op = puffs_specop_p;
		spec_node_init(vp, rdev);
	} else if (type == VFIFO) {
		vp->v_op = puffs_fifoop_p;
	} else if (vp->v_type == VREG) {
		uvm_vnp_setsize(vp, vsize);
	}

	pnode->pn_serversize = vsize;

	DPRINTF(("new vnode at %p, pnode %p, cookie %p\n", vp,
	    pnode, pnode->pn_cookie));

	mutex_exit(&pnode->pn_mtx);

	return 0;
}

int
puffs_getvnode(struct mount *mp, puffs_cookie_t ck, enum vtype type,
	voff_t vsize, dev_t rdev, struct vnode **vpp)
{

	return puffs_getvnode1(mp, ck, type, vsize, rdev, true, vpp);
}

/* new node creating for creative vop ops (create, symlink, mkdir, mknod) */
int
puffs_newnode(struct mount *mp, struct vnode *dvp, struct vnode **vpp,
	puffs_cookie_t ck, struct componentname *cnp,
	enum vtype type, dev_t rdev)
{
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	int error;

	/* userspace probably has this as a NULL op */
	if (ck == NULL)
		return EOPNOTSUPP;

	/*
	 * Check for previous node with the same designation.
	 * Explicitly check the root node cookie, since it might be
	 * reclaimed from the kernel when this check is made.
	 */
	if (ck == pmp->pmp_root_cookie) {
		puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EEXIST,
		    "cookie exists", ck);
		return EPROTO;
	}

	KASSERT(curlwp != uvm.pagedaemon_lwp);

	error = puffs_getvnode1(dvp->v_mount, ck, type, 0, rdev, false, vpp);
	if (error) {
		if (error == EEXIST) {
			puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EEXIST,
			    "cookie exists", ck);
			error = EPROTO;
		}
		return error;
	}

	if (PUFFS_USE_NAMECACHE(pmp))
		cache_enter(dvp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

	puffs_updatenode(VPTOPP(dvp), PUFFS_UPDATECTIME|PUFFS_UPDATEMTIME, 0);

	return 0;
}

void
puffs_putvnode(struct vnode *vp)
{
	struct puffs_node *pnode;

	pnode = VPTOPP(vp);

	KASSERT(vp->v_tag == VT_PUFFS);

	vcache_remove(vp->v_mount, &pnode->pn_cookie, sizeof(pnode->pn_cookie));
	genfs_node_destroy(vp);
	
	/*
	 * To interlock with puffs_getvnode1().
	 */
	mutex_enter(vp->v_interlock);
	vp->v_data = NULL;
	mutex_exit(vp->v_interlock);
	puffs_releasenode(pnode);
}

/*
 * Make sure root vnode exists and reference it.  Does NOT lock.
 */
static int
puffs_makeroot(struct puffs_mount *pmp)
{
	struct vnode *vp;
	int rv;

	/*
	 * pmp_lock must be held if vref()'ing or vrele()'ing the
	 * root vnode.  the latter is controlled by puffs_inactive().
	 *
	 * pmp_root is set here and cleared in puffs_reclaim().
	 */

	rv = puffs_getvnode(pmp->pmp_mp, pmp->pmp_root_cookie,
	    pmp->pmp_root_vtype, pmp->pmp_root_vsize, pmp->pmp_root_rdev, &vp);
	if (rv != 0)
		return rv;

	mutex_enter(&pmp->pmp_lock);
	if (pmp->pmp_root == NULL)
		pmp->pmp_root = vp;
	mutex_exit(&pmp->pmp_lock);

	return 0;
}

/*
 * Locate the in-kernel vnode based on the cookie received given
 * from userspace.
 *
 * returns 0 on success.  otherwise returns an errno or PUFFS_NOSUCHCOOKIE.
 *
 * returns PUFFS_NOSUCHCOOKIE if no vnode for the cookie is found.
 */
int
puffs_cookie2vnode(struct puffs_mount *pmp, puffs_cookie_t ck,
    struct vnode **vpp)
{
	int rv;

	/*
	 * Handle root in a special manner, since we want to make sure
	 * pmp_root is properly set.
	 */
	if (ck == pmp->pmp_root_cookie) {
		if ((rv = puffs_makeroot(pmp)))
			return rv;
		*vpp = pmp->pmp_root;
		return 0;
	}

	rv = vcache_get(PMPTOMP(pmp), &ck, sizeof(ck), vpp);
	if (rv != 0)
		return rv;
	mutex_enter((*vpp)->v_interlock);
	if ((*vpp)->v_type == VNON) {
		mutex_exit((*vpp)->v_interlock);
		/* XXX vrele() calls VOP_INACTIVE() with VNON node */
		vrele(*vpp);
		*vpp = NULL;
		return PUFFS_NOSUCHCOOKIE;
	}
	mutex_exit((*vpp)->v_interlock);

	return 0;
}

void
puffs_updatenode(struct puffs_node *pn, int flags, voff_t size)
{
	struct timespec ts;

	if (flags == 0)
		return;

	nanotime(&ts);

	if (flags & PUFFS_UPDATEATIME) {
		pn->pn_mc_atime = ts;
		pn->pn_stat |= PNODE_METACACHE_ATIME;
	}
	if (flags & PUFFS_UPDATECTIME) {
		pn->pn_mc_ctime = ts;
		pn->pn_stat |= PNODE_METACACHE_CTIME;
	}
	if (flags & PUFFS_UPDATEMTIME) {
		pn->pn_mc_mtime = ts;
		pn->pn_stat |= PNODE_METACACHE_MTIME;
	}
	if (flags & PUFFS_UPDATESIZE) {
		pn->pn_mc_size = size;
		pn->pn_stat |= PNODE_METACACHE_SIZE;
	}
}

/*
 * Add reference to node.
 *  mutex held on entry and return
 */
void
puffs_referencenode(struct puffs_node *pn)
{

	KASSERT(mutex_owned(&pn->pn_mtx));
	pn->pn_refcount++;
}

/*
 * Release pnode structure which dealing with references to the
 * puffs_node instead of the vnode.  Can't use vref()/vrele() on
 * the vnode there, since that causes the lovely VOP_INACTIVE(),
 * which in turn causes the lovely deadlock when called by the one
 * who is supposed to handle it.
 */
void
puffs_releasenode(struct puffs_node *pn)
{

	mutex_enter(&pn->pn_mtx);
	if (--pn->pn_refcount == 0) {
		mutex_exit(&pn->pn_mtx);
		mutex_destroy(&pn->pn_mtx);
		mutex_destroy(&pn->pn_sizemtx);
		seldestroy(&pn->pn_sel);
		if (pn->pn_va_cache != NULL)
			pool_put(&puffs_vapool, pn->pn_va_cache);
		pool_put(&puffs_pnpool, pn);
	} else {
		mutex_exit(&pn->pn_mtx);
	}
}
