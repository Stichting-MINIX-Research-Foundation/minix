/*	$NetBSD: puffs_node.c,v 1.30 2013/10/17 21:03:27 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: puffs_node.c,v 1.30 2013/10/17 21:03:27 christos Exp $");

#include <sys/param.h>
#include <sys/hash.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>

#include <uvm/uvm.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#include <miscfs/genfs/genfs_node.h>
#include <miscfs/specfs/specdev.h>

static const struct genfs_ops puffs_genfsops = {
	.gop_size = puffs_gop_size,
	.gop_write = genfs_gop_write,
	.gop_markupdate = puffs_gop_markupdate,
#if 0
	.gop_alloc, should ask userspace
#endif
};

static __inline struct puffs_node_hashlist
	*puffs_cookie2hashlist(struct puffs_mount *, puffs_cookie_t);
static struct puffs_node *puffs_cookie2pnode(struct puffs_mount *,
					     puffs_cookie_t);

struct pool puffs_pnpool;
struct pool puffs_vapool;

/*
 * Grab a vnode, intialize all the puffs-dependent stuff.
 */
int
puffs_getvnode(struct mount *mp, puffs_cookie_t ck, enum vtype type,
	voff_t vsize, dev_t rdev, struct vnode **vpp)
{
	struct puffs_mount *pmp;
	struct puffs_newcookie *pnc;
	struct vnode *vp;
	struct puffs_node *pnode;
	struct puffs_node_hashlist *plist;
	int error;

	pmp = MPTOPUFFSMP(mp);

	error = EPROTO;
	if (type <= VNON || type >= VBAD) {
		puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EINVAL,
		    "bad node type", ck);
		goto bad;
	}
	if (vsize == VSIZENOTSET) {
		puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EINVAL,
		    "VSIZENOTSET is not a valid size", ck);
		goto bad;
	}

	error = getnewvnode(VT_PUFFS, mp, puffs_vnodeop_p, NULL, &vp);
	if (error) {
		goto bad;
	}
	vp->v_type = type;

	/*
	 * Creation should not fail after this point.  Or if it does,
	 * care must be taken so that VOP_INACTIVE() isn't called.
	 */

	/* default size */
	uvm_vnp_setsize(vp, 0);

	/* dances based on vnode type. almost ufs_vinit(), but not quite */
	switch (type) {
	case VCHR:
	case VBLK:
		/*
		 * replace vnode operation vector with the specops vector.
		 * our user server has very little control over the node
		 * if it decides its a character or block special file
		 */
		vp->v_op = puffs_specop_p;
		spec_node_init(vp, rdev);
		break;

	case VFIFO:
		vp->v_op = puffs_fifoop_p;
		break;

	case VREG:
		uvm_vnp_setsize(vp, vsize);
		break;

	case VDIR:
	case VLNK:
	case VSOCK:
		break;
	default:
		panic("puffs_getvnode: invalid vtype %d", type);
	}

	pnode = pool_get(&puffs_pnpool, PR_WAITOK);
	memset(pnode, 0, sizeof(struct puffs_node));

	pnode->pn_cookie = ck;
	pnode->pn_refcount = 1;

	/* insert cookie on list, take off of interlock list */
	mutex_init(&pnode->pn_mtx, MUTEX_DEFAULT, IPL_NONE);
	selinit(&pnode->pn_sel);
	plist = puffs_cookie2hashlist(pmp, ck);
	mutex_enter(&pmp->pmp_lock);
	LIST_INSERT_HEAD(plist, pnode, pn_hashent);
	if (ck != pmp->pmp_root_cookie) {
		LIST_FOREACH(pnc, &pmp->pmp_newcookie, pnc_entries) {
			if (pnc->pnc_cookie == ck) {
				LIST_REMOVE(pnc, pnc_entries);
				kmem_free(pnc, sizeof(struct puffs_newcookie));
				break;
			}
		}
		KASSERT(pnc != NULL);
	}
	mutex_init(&pnode->pn_sizemtx, MUTEX_DEFAULT, IPL_NONE);
	mutex_exit(&pmp->pmp_lock);

	vp->v_data = pnode;
	vp->v_type = type;
	pnode->pn_vp = vp;
	pnode->pn_serversize = vsize;

	genfs_node_init(vp, &puffs_genfsops);
	*vpp = vp;

	DPRINTF(("new vnode at %p, pnode %p, cookie %p\n", vp,
	    pnode, pnode->pn_cookie));

	return 0;

 bad:
	/* remove staging cookie from list */
	if (ck != pmp->pmp_root_cookie) {
		mutex_enter(&pmp->pmp_lock);
		LIST_FOREACH(pnc, &pmp->pmp_newcookie, pnc_entries) {
			if (pnc->pnc_cookie == ck) {
				LIST_REMOVE(pnc, pnc_entries);
				kmem_free(pnc, sizeof(struct puffs_newcookie));
				break;
			}
		}
		KASSERT(pnc != NULL);
		mutex_exit(&pmp->pmp_lock);
	}

	return error;
}

/* new node creating for creative vop ops (create, symlink, mkdir, mknod) */
int
puffs_newnode(struct mount *mp, struct vnode *dvp, struct vnode **vpp,
	puffs_cookie_t ck, struct componentname *cnp,
	enum vtype type, dev_t rdev)
{
	struct puffs_mount *pmp = MPTOPUFFSMP(mp);
	struct puffs_newcookie *pnc;
	struct vnode *vp;
	int error;

	/* userspace probably has this as a NULL op */
	if (ck == NULL) {
		error = EOPNOTSUPP;
		return error;
	}

	/*
	 * Check for previous node with the same designation.
	 * Explicitly check the root node cookie, since it might be
	 * reclaimed from the kernel when this check is made.
	 */
	mutex_enter(&pmp->pmp_lock);
	if (ck == pmp->pmp_root_cookie
	    || puffs_cookie2pnode(pmp, ck) != NULL) {
		mutex_exit(&pmp->pmp_lock);
		puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EEXIST,
		    "cookie exists", ck);
		return EPROTO;
	}

	LIST_FOREACH(pnc, &pmp->pmp_newcookie, pnc_entries) {
		if (pnc->pnc_cookie == ck) {
			mutex_exit(&pmp->pmp_lock);
			puffs_senderr(pmp, PUFFS_ERR_MAKENODE, EEXIST,
			    "newcookie exists", ck);
			return EPROTO;
		}
	}

	KASSERT(curlwp != uvm.pagedaemon_lwp);
	pnc = kmem_alloc(sizeof(struct puffs_newcookie), KM_SLEEP);
	pnc->pnc_cookie = ck;
	LIST_INSERT_HEAD(&pmp->pmp_newcookie, pnc, pnc_entries);
	mutex_exit(&pmp->pmp_lock);

	error = puffs_getvnode(dvp->v_mount, ck, type, 0, rdev, &vp);
	if (error)
		return error;

	vp->v_type = type;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	*vpp = vp;

	if (PUFFS_USE_NAMECACHE(pmp))
		cache_enter(dvp, vp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

	return 0;
}

void
puffs_putvnode(struct vnode *vp)
{
	struct puffs_node *pnode;

	pnode = VPTOPP(vp);

#ifdef DIAGNOSTIC
	if (vp->v_tag != VT_PUFFS)
		panic("puffs_putvnode: %p not a puffs vnode", vp);
#endif

	genfs_node_destroy(vp);
	puffs_releasenode(pnode);
	vp->v_data = NULL;

	return;
}

static __inline struct puffs_node_hashlist *
puffs_cookie2hashlist(struct puffs_mount *pmp, puffs_cookie_t ck)
{
	uint32_t hash;

	hash = hash32_buf(&ck, sizeof(ck), HASH32_BUF_INIT);
	return &pmp->pmp_pnodehash[hash % pmp->pmp_npnodehash];
}

/*
 * Translate cookie to puffs_node.  Caller must hold pmp_lock
 * and it will be held upon return.
 */
static struct puffs_node *
puffs_cookie2pnode(struct puffs_mount *pmp, puffs_cookie_t ck)
{
	struct puffs_node_hashlist *plist;
	struct puffs_node *pnode;

	plist = puffs_cookie2hashlist(pmp, ck);
	LIST_FOREACH(pnode, plist, pn_hashent) {
		if (pnode->pn_cookie == ck)
			break;
	}

	return pnode;
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
 retry:
	mutex_enter(&pmp->pmp_lock);
	vp = pmp->pmp_root;
	if (vp) {
		mutex_enter(vp->v_interlock);
		mutex_exit(&pmp->pmp_lock);
		switch (vget(vp, 0)) {
		case ENOENT:
			goto retry;
		case 0:
			return 0;
		default:
			break;
		}
	} else
		mutex_exit(&pmp->pmp_lock);

	/*
	 * So, didn't have the magic root vnode available.
	 * No matter, grab another and stuff it with the cookie.
	 */
	if ((rv = puffs_getvnode(pmp->pmp_mp, pmp->pmp_root_cookie,
	    pmp->pmp_root_vtype, pmp->pmp_root_vsize, pmp->pmp_root_rdev, &vp)))
		return rv;

	/*
	 * Someone magically managed to race us into puffs_getvnode?
	 * Put our previous new vnode back and retry.
	 */
	mutex_enter(&pmp->pmp_lock);
	if (pmp->pmp_root) {
		struct puffs_node *pnode = vp->v_data;

		LIST_REMOVE(pnode, pn_hashent);
		mutex_exit(&pmp->pmp_lock);
		puffs_putvnode(vp);
		goto retry;
	} 

	/* store cache */
	vp->v_vflag |= VV_ROOT;
	pmp->pmp_root = vp;
	mutex_exit(&pmp->pmp_lock);

	return 0;
}

/*
 * Locate the in-kernel vnode based on the cookie received given
 * from userspace.
 * The parameter "lock" control whether to lock the possible or
 * not.  Locking always might cause us to lock against ourselves
 * in situations where we want the vnode but don't care for the
 * vnode lock, e.g. file server issued putpages.
 *
 * returns 0 on success.  otherwise returns an errno or PUFFS_NOSUCHCOOKIE.
 *
 * returns PUFFS_NOSUCHCOOKIE if no vnode for the cookie is found.
 * in that case, if willcreate=true, the pmp_newcookie list is populated with
 * the given cookie.  it's the caller's responsibility to consume the entry
 * with calling puffs_getvnode.
 */
int
puffs_cookie2vnode(struct puffs_mount *pmp, puffs_cookie_t ck, int lock,
	int willcreate, struct vnode **vpp)
{
	struct puffs_node *pnode;
	struct puffs_newcookie *pnc;
	struct vnode *vp;
	int vgetflags, rv;

	/*
	 * Handle root in a special manner, since we want to make sure
	 * pmp_root is properly set.
	 */
	if (ck == pmp->pmp_root_cookie) {
		if ((rv = puffs_makeroot(pmp)))
			return rv;
		if (lock)
			vn_lock(pmp->pmp_root, LK_EXCLUSIVE | LK_RETRY);

		*vpp = pmp->pmp_root;
		return 0;
	}

 retry:
	mutex_enter(&pmp->pmp_lock);
	pnode = puffs_cookie2pnode(pmp, ck);
	if (pnode == NULL) {
		if (willcreate) {
			pnc = kmem_alloc(sizeof(struct puffs_newcookie),
			    KM_SLEEP);
			pnc->pnc_cookie = ck;
			LIST_INSERT_HEAD(&pmp->pmp_newcookie, pnc, pnc_entries);
		}
		mutex_exit(&pmp->pmp_lock);
		return PUFFS_NOSUCHCOOKIE;
	}
	vp = pnode->pn_vp;
	mutex_enter(vp->v_interlock);
	mutex_exit(&pmp->pmp_lock);

	vgetflags = 0;
	if (lock)
		vgetflags |= LK_EXCLUSIVE;
	switch (rv = vget(vp, vgetflags)) {
	case ENOENT:
		goto retry;
	case 0:
		break;
	default:
		return rv;
	}

	*vpp = vp;
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
