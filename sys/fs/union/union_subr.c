/*	$NetBSD: union_subr.c,v 1.73 2015/04/20 19:36:55 riastradh Exp $	*/

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)union_subr.c	8.20 (Berkeley) 5/20/95
 */

/*
 * Copyright (c) 1994 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)union_subr.c	8.20 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: union_subr.c,v 1.73 2015/04/20 19:36:55 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#include <fs/union/union.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/specfs/specdev.h>

static LIST_HEAD(uhashhead, union_node) *uhashtbl;
static u_long uhash_mask;		/* size of hash table - 1 */
#define UNION_HASH(u, l) \
	((((u_long) (u) + (u_long) (l)) >> 8) & uhash_mask)
#define NOHASH	((u_long)-1)

static kmutex_t uhash_lock;

void union_updatevp(struct union_node *, struct vnode *, struct vnode *);
static void union_ref(struct union_node *);
static void union_rele(struct union_node *);
static int union_do_lookup(struct vnode *, struct componentname *, kauth_cred_t,    const char *);
int union_vn_close(struct vnode *, int, kauth_cred_t, struct lwp *);
static void union_dircache_r(struct vnode *, struct vnode ***, int *);
struct vnode *union_dircache(struct vnode *, struct lwp *);

void
union_init(void)
{

	mutex_init(&uhash_lock, MUTEX_DEFAULT, IPL_NONE);
	uhashtbl = hashinit(desiredvnodes, HASH_LIST, true, &uhash_mask);
}

void
union_reinit(void)
{
	struct union_node *un;
	struct uhashhead *oldhash, *hash;
	u_long oldmask, mask, val;
	int i;

	hash = hashinit(desiredvnodes, HASH_LIST, true, &mask);
	mutex_enter(&uhash_lock);
	oldhash = uhashtbl;
	oldmask = uhash_mask;
	uhashtbl = hash;
	uhash_mask = mask;
	for (i = 0; i <= oldmask; i++) {
		while ((un = LIST_FIRST(&oldhash[i])) != NULL) {
			LIST_REMOVE(un, un_cache);
			val = UNION_HASH(un->un_uppervp, un->un_lowervp);
			LIST_INSERT_HEAD(&hash[val], un, un_cache);
		}
	}
	mutex_exit(&uhash_lock);
	hashdone(oldhash, HASH_LIST, oldmask);
}

/*
 * Free global unionfs resources.
 */
void
union_done(void)
{

	hashdone(uhashtbl, HASH_LIST, uhash_mask);
	mutex_destroy(&uhash_lock);

	/* Make sure to unset the readdir hook. */
	vn_union_readdir_hook = NULL;
}

void
union_updatevp(struct union_node *un, struct vnode *uppervp,
	struct vnode *lowervp)
{
	int ohash = UNION_HASH(un->un_uppervp, un->un_lowervp);
	int nhash = UNION_HASH(uppervp, lowervp);
	int docache = (lowervp != NULLVP || uppervp != NULLVP);
	bool un_unlock;

	KASSERT(VOP_ISLOCKED(UNIONTOV(un)) == LK_EXCLUSIVE);

	mutex_enter(&uhash_lock);

	if (!docache || ohash != nhash) {
		if (un->un_cflags & UN_CACHED) {
			un->un_cflags &= ~UN_CACHED;
			LIST_REMOVE(un, un_cache);
		}
	}

	if (un->un_lowervp != lowervp) {
		if (un->un_lowervp) {
			vrele(un->un_lowervp);
			if (un->un_path) {
				free(un->un_path, M_TEMP);
				un->un_path = 0;
			}
			if (un->un_dirvp) {
				vrele(un->un_dirvp);
				un->un_dirvp = NULLVP;
			}
		}
		un->un_lowervp = lowervp;
		mutex_enter(&un->un_lock);
		un->un_lowersz = VNOVAL;
		mutex_exit(&un->un_lock);
	}

	if (un->un_uppervp != uppervp) {
		if (un->un_uppervp) {
			un_unlock = false;
			vrele(un->un_uppervp);
		} else
			un_unlock = true;

		mutex_enter(&un->un_lock);
		un->un_uppervp = uppervp;
		mutex_exit(&un->un_lock);
		if (un_unlock) {
			struct vop_unlock_args ap;

			ap.a_vp = UNIONTOV(un);
			genfs_unlock(&ap);
		}
		mutex_enter(&un->un_lock);
		un->un_uppersz = VNOVAL;
		mutex_exit(&un->un_lock);
		/* Update union vnode interlock. */
		if (uppervp != NULL) {
			mutex_obj_hold(uppervp->v_interlock);
			uvm_obj_setlock(&UNIONTOV(un)->v_uobj,
			    uppervp->v_interlock);
		}
	}

	if (docache && (ohash != nhash)) {
		LIST_INSERT_HEAD(&uhashtbl[nhash], un, un_cache);
		un->un_cflags |= UN_CACHED;
	}

	mutex_exit(&uhash_lock);
}

void
union_newlower(struct union_node *un, struct vnode *lowervp)
{

	union_updatevp(un, un->un_uppervp, lowervp);
}

void
union_newupper(struct union_node *un, struct vnode *uppervp)
{

	union_updatevp(un, uppervp, un->un_lowervp);
}

/*
 * Keep track of size changes in the underlying vnodes.
 * If the size changes, then callback to the vm layer
 * giving priority to the upper layer size.
 *
 * Mutex un_lock hold on entry and released on return.
 */
void
union_newsize(struct vnode *vp, off_t uppersz, off_t lowersz)
{
	struct union_node *un = VTOUNION(vp);
	off_t sz;

	KASSERT(mutex_owned(&un->un_lock));
	/* only interested in regular files */
	if (vp->v_type != VREG) {
		mutex_exit(&un->un_lock);
		uvm_vnp_setsize(vp, 0);
		return;
	}

	sz = VNOVAL;

	if ((uppersz != VNOVAL) && (un->un_uppersz != uppersz)) {
		un->un_uppersz = uppersz;
		if (sz == VNOVAL)
			sz = un->un_uppersz;
	}

	if ((lowersz != VNOVAL) && (un->un_lowersz != lowersz)) {
		un->un_lowersz = lowersz;
		if (sz == VNOVAL)
			sz = un->un_lowersz;
	}
	mutex_exit(&un->un_lock);

	if (sz != VNOVAL) {
#ifdef UNION_DIAGNOSTIC
		printf("union: %s size now %qd\n",
		    uppersz != VNOVAL ? "upper" : "lower", sz);
#endif
		uvm_vnp_setsize(vp, sz);
	}
}

static void
union_ref(struct union_node *un)
{

	KASSERT(mutex_owned(&uhash_lock));
	un->un_refs++;
}

static void
union_rele(struct union_node *un)
{

	mutex_enter(&uhash_lock);
	un->un_refs--;
	if (un->un_refs > 0) {
		mutex_exit(&uhash_lock);
		return;
	}
	if (un->un_cflags & UN_CACHED) {
		un->un_cflags &= ~UN_CACHED;
		LIST_REMOVE(un, un_cache);
	}
	mutex_exit(&uhash_lock);

	if (un->un_pvp != NULLVP)
		vrele(un->un_pvp);
	if (un->un_uppervp != NULLVP)
		vrele(un->un_uppervp);
	if (un->un_lowervp != NULLVP)
		vrele(un->un_lowervp);
	if (un->un_dirvp != NULLVP)
		vrele(un->un_dirvp);
	if (un->un_path)
		free(un->un_path, M_TEMP);
	mutex_destroy(&un->un_lock);

	free(un, M_TEMP);
}

/*
 * allocate a union_node/vnode pair.  the vnode is
 * referenced and unlocked.  the new vnode is returned
 * via (vpp).  (mp) is the mountpoint of the union filesystem,
 * (dvp) is the parent directory where the upper layer object
 * should exist (but doesn't) and (cnp) is the componentname
 * information which is partially copied to allow the upper
 * layer object to be created at a later time.  (uppervp)
 * and (lowervp) reference the upper and lower layer objects
 * being mapped.  either, but not both, can be nil.
 * both, if supplied, are unlocked.
 * the reference is either maintained in the new union_node
 * object which is allocated, or they are vrele'd.
 *
 * all union_nodes are maintained on a hash
 * list.  new nodes are only allocated when they cannot
 * be found on this list.  entries on the list are
 * removed when the vfs reclaim entry is called.
 *
 * the vnode gets attached or referenced with vcache_get().
 */
int
union_allocvp(
	struct vnode **vpp,
	struct mount *mp,
	struct vnode *undvp,		/* parent union vnode */
	struct vnode *dvp,		/* may be null */
	struct componentname *cnp,	/* may be null */
	struct vnode *uppervp,		/* may be null */
	struct vnode *lowervp,		/* may be null */
	int docache)
{
	int error;
	struct union_node *un = NULL, *un1;
	struct vnode *vp, *xlowervp = NULLVP;
	u_long hash[3];
	int try;
	bool is_dotdot;

	is_dotdot = (dvp != NULL && cnp != NULL && (cnp->cn_flags & ISDOTDOT));

	if (uppervp == NULLVP && lowervp == NULLVP)
		panic("union: unidentifiable allocation");

	if (uppervp && lowervp && (uppervp->v_type != lowervp->v_type)) {
		xlowervp = lowervp;
		lowervp = NULLVP;
	}

	if (!docache) {
		un = NULL;
		goto found;
	}

	/*
	 * If both uppervp and lowervp are not NULL we have to
	 * search union nodes with one vnode as NULL too.
	 */
	hash[0] = UNION_HASH(uppervp, lowervp);
	if (uppervp == NULL || lowervp == NULL) {
		hash[1] = hash[2] = NOHASH;
	} else {
		hash[1] = UNION_HASH(uppervp, NULLVP);
		hash[2] = UNION_HASH(NULLVP, lowervp);
	}

loop:
	mutex_enter(&uhash_lock);

	for (try = 0; try < 3; try++) {
		if (hash[try] == NOHASH)
			continue;
		LIST_FOREACH(un, &uhashtbl[hash[try]], un_cache) {
			if ((un->un_lowervp && un->un_lowervp != lowervp) ||
			    (un->un_uppervp && un->un_uppervp != uppervp) ||
			    un->un_mount != mp)
				continue;

			union_ref(un);
			mutex_exit(&uhash_lock);
			error = vcache_get(mp, &un, sizeof(un), &vp);
			KASSERT(error != 0 || UNIONTOV(un) == vp);
			union_rele(un);
			if (error == ENOENT)
				goto loop;
			else if (error)
				goto out;
			goto found;
		}
	}

	mutex_exit(&uhash_lock);

found:
	if (un) {
		if (uppervp != dvp) {
			if (is_dotdot)
				VOP_UNLOCK(dvp);
			vn_lock(UNIONTOV(un), LK_EXCLUSIVE | LK_RETRY);
			if (is_dotdot)
				vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		}
		/*
		 * Save information about the upper layer.
		 */
		if (uppervp != un->un_uppervp) {
			union_newupper(un, uppervp);
		} else if (uppervp) {
			vrele(uppervp);
		}

		/*
		 * Save information about the lower layer.
		 * This needs to keep track of pathname
		 * and directory information which union_vn_create
		 * might need.
		 */
		if (lowervp != un->un_lowervp) {
			union_newlower(un, lowervp);
			if (cnp && (lowervp != NULLVP)) {
				un->un_path = malloc(cnp->cn_namelen+1,
						M_TEMP, M_WAITOK);
				memcpy(un->un_path, cnp->cn_nameptr,
						cnp->cn_namelen);
				un->un_path[cnp->cn_namelen] = '\0';
				vref(dvp);
				un->un_dirvp = dvp;
			}
		} else if (lowervp) {
			vrele(lowervp);
		}
		*vpp = UNIONTOV(un);
		if (uppervp != dvp)
			VOP_UNLOCK(*vpp);
		error = 0;
		goto out;
	}

	un = malloc(sizeof(struct union_node), M_TEMP, M_WAITOK);
	mutex_init(&un->un_lock, MUTEX_DEFAULT, IPL_NONE);
	un->un_refs = 1;
	un->un_mount = mp;
	un->un_vnode = NULL;
	un->un_uppervp = uppervp;
	un->un_lowervp = lowervp;
	un->un_pvp = undvp;
	if (undvp != NULLVP)
		vref(undvp);
	un->un_dircache = 0;
	un->un_openl = 0;
	un->un_cflags = 0;

	un->un_uppersz = VNOVAL;
	un->un_lowersz = VNOVAL;

	if (dvp && cnp && (lowervp != NULLVP)) {
		un->un_path = malloc(cnp->cn_namelen+1, M_TEMP, M_WAITOK);
		memcpy(un->un_path, cnp->cn_nameptr, cnp->cn_namelen);
		un->un_path[cnp->cn_namelen] = '\0';
		vref(dvp);
		un->un_dirvp = dvp;
	} else {
		un->un_path = 0;
		un->un_dirvp = 0;
	}

	if (docache) {
		mutex_enter(&uhash_lock);
		LIST_FOREACH(un1, &uhashtbl[hash[0]], un_cache) {
			if (un1->un_lowervp == lowervp &&
			    un1->un_uppervp == uppervp &&
			    un1->un_mount == mp) {
				/*
				 * Another thread beat us, push back freshly
				 * allocated node and retry.
				 */
				mutex_exit(&uhash_lock);
				union_rele(un);
				goto loop;
			}
		}
		LIST_INSERT_HEAD(&uhashtbl[hash[0]], un, un_cache);
		un->un_cflags |= UN_CACHED;
		mutex_exit(&uhash_lock);
	}

	error = vcache_get(mp, &un, sizeof(un), vpp);
	KASSERT(error != 0 || UNIONTOV(un) == *vpp);
	union_rele(un);
	if (error == ENOENT)
		goto loop;

out:
	if (xlowervp)
		vrele(xlowervp);

	return error;
}

int
union_freevp(struct vnode *vp)
{
	struct union_node *un = VTOUNION(vp);

	/* Detach vnode from union node. */
	un->un_vnode = NULL;
	un->un_uppersz = VNOVAL;
	un->un_lowersz = VNOVAL;

	vcache_remove(vp->v_mount, &un, sizeof(un));

	/* Detach union node from vnode. */
	mutex_enter(vp->v_interlock);
	vp->v_data = NULL;
	mutex_exit(vp->v_interlock);

	union_rele(un);

	return 0;
}

int
union_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	struct vattr va;
	struct vnode *svp;
	struct union_node *un;
	struct union_mount *um;
	voff_t uppersz, lowersz;

	KASSERT(key_len == sizeof(un));
	memcpy(&un, key, key_len);

	um = MOUNTTOUNIONMOUNT(mp);
	svp = (un->un_uppervp != NULLVP) ? un->un_uppervp : un->un_lowervp;

	vp->v_tag = VT_UNION;
	vp->v_op = union_vnodeop_p;
	vp->v_data = un;
	un->un_vnode = vp;

	vp->v_type = svp->v_type;
	if (svp->v_type == VCHR || svp->v_type == VBLK)
		spec_node_init(vp, svp->v_rdev);

	mutex_obj_hold(svp->v_interlock);
	uvm_obj_setlock(&vp->v_uobj, svp->v_interlock);

	/* detect the root vnode (and aliases) */
	if ((un->un_uppervp == um->um_uppervp) &&
	    ((un->un_lowervp == NULLVP) || un->un_lowervp == um->um_lowervp)) {
		if (un->un_lowervp == NULLVP) {
			un->un_lowervp = um->um_lowervp;
			if (un->un_lowervp != NULLVP) 
				vref(un->un_lowervp);
		}
		vp->v_vflag |= VV_ROOT;
	}

	uppersz = lowersz = VNOVAL;
	if (un->un_uppervp != NULLVP) {
		if (vn_lock(un->un_uppervp, LK_SHARED) == 0) {
			if (VOP_GETATTR(un->un_uppervp, &va, FSCRED) == 0)
				uppersz = va.va_size;
			VOP_UNLOCK(un->un_uppervp);
		}
	}
	if (un->un_lowervp != NULLVP) {
		if (vn_lock(un->un_lowervp, LK_SHARED) == 0) {
			if (VOP_GETATTR(un->un_lowervp, &va, FSCRED) == 0)
				lowersz = va.va_size;
			VOP_UNLOCK(un->un_lowervp);
		}
	}

	mutex_enter(&un->un_lock);
	union_newsize(vp, uppersz, lowersz);

	mutex_enter(&uhash_lock);
	union_ref(un);
	mutex_exit(&uhash_lock);

	*new_key = &vp->v_data;

	return 0;
}

/*
 * copyfile.  copy the vnode (fvp) to the vnode (tvp)
 * using a sequence of reads and writes.  both (fvp)
 * and (tvp) are locked on entry and exit.
 */
int
union_copyfile(struct vnode *fvp, struct vnode *tvp, kauth_cred_t cred,
	struct lwp *l)
{
	char *tbuf;
	struct uio uio;
	struct iovec iov;
	int error = 0;

	/*
	 * strategy:
	 * allocate a buffer of size MAXBSIZE.
	 * loop doing reads and writes, keeping track
	 * of the current uio offset.
	 * give up at the first sign of trouble.
	 */

	uio.uio_offset = 0;
	UIO_SETUP_SYSSPACE(&uio);

	tbuf = malloc(MAXBSIZE, M_TEMP, M_WAITOK);

	/* ugly loop follows... */
	do {
		off_t offset = uio.uio_offset;

		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		iov.iov_base = tbuf;
		iov.iov_len = MAXBSIZE;
		uio.uio_resid = iov.iov_len;
		uio.uio_rw = UIO_READ;
		error = VOP_READ(fvp, &uio, 0, cred);

		if (error == 0) {
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			iov.iov_base = tbuf;
			iov.iov_len = MAXBSIZE - uio.uio_resid;
			uio.uio_offset = offset;
			uio.uio_rw = UIO_WRITE;
			uio.uio_resid = iov.iov_len;

			if (uio.uio_resid == 0)
				break;

			do {
				error = VOP_WRITE(tvp, &uio, 0, cred);
			} while ((uio.uio_resid > 0) && (error == 0));
		}

	} while (error == 0);

	free(tbuf, M_TEMP);
	return (error);
}

/*
 * (un) is assumed to be locked on entry and remains
 * locked on exit.
 */
int
union_copyup(struct union_node *un, int docopy, kauth_cred_t cred,
	struct lwp *l)
{
	int error;
	struct vnode *lvp, *uvp;
	struct vattr lvattr, uvattr;

	error = union_vn_create(&uvp, un, l);
	if (error)
		return (error);

	KASSERT(VOP_ISLOCKED(uvp) == LK_EXCLUSIVE);
	union_newupper(un, uvp);

	lvp = un->un_lowervp;

	if (docopy) {
		/*
		 * XX - should not ignore errors
		 * from VOP_CLOSE
		 */
		vn_lock(lvp, LK_EXCLUSIVE | LK_RETRY);

        	error = VOP_GETATTR(lvp, &lvattr, cred);
		if (error == 0)
			error = VOP_OPEN(lvp, FREAD, cred);
		if (error == 0) {
			error = union_copyfile(lvp, uvp, cred, l);
			(void) VOP_CLOSE(lvp, FREAD, cred);
		}
		if (error == 0) {
			/* Copy permissions up too */
			vattr_null(&uvattr);
			uvattr.va_mode = lvattr.va_mode;
			uvattr.va_flags = lvattr.va_flags;
        		error = VOP_SETATTR(uvp, &uvattr, cred);
		}
		VOP_UNLOCK(lvp);
#ifdef UNION_DIAGNOSTIC
		if (error == 0)
			uprintf("union: copied up %s\n", un->un_path);
#endif

	}
	union_vn_close(uvp, FWRITE, cred, l);

	/*
	 * Subsequent IOs will go to the top layer, so
	 * call close on the lower vnode and open on the
	 * upper vnode to ensure that the filesystem keeps
	 * its references counts right.  This doesn't do
	 * the right thing with (cred) and (FREAD) though.
	 * Ignoring error returns is not right, either.
	 */
	if (error == 0) {
		int i;

		vn_lock(lvp, LK_EXCLUSIVE | LK_RETRY);
		for (i = 0; i < un->un_openl; i++) {
			(void) VOP_CLOSE(lvp, FREAD, cred);
			(void) VOP_OPEN(uvp, FREAD, cred);
		}
		un->un_openl = 0;
		VOP_UNLOCK(lvp);
	}

	return (error);

}

/*
 * Prepare the creation of a new node in the upper layer.
 *
 * (dvp) is the directory in which to create the new node.
 * it is locked on entry and exit.
 * (cnp) is the componentname to be created.
 * (cred, path, hash) are credentials, path and its hash to fill (cnp).
 */
static int
union_do_lookup(struct vnode *dvp, struct componentname *cnp, kauth_cred_t cred,
    const char *path)
{
	int error;
	struct vnode *vp;

	cnp->cn_nameiop = CREATE;
	cnp->cn_flags = LOCKPARENT | ISLASTCN;
	cnp->cn_cred = cred;
	cnp->cn_nameptr = path;
	cnp->cn_namelen = strlen(path);

	error = VOP_LOOKUP(dvp, &vp, cnp);

	if (error == 0) {
		KASSERT(vp != NULL);
		VOP_ABORTOP(dvp, cnp);
		vrele(vp);
		error = EEXIST;
	} else if (error == EJUSTRETURN) {
		error = 0;
	}

	return error;
}

/*
 * Create a shadow directory in the upper layer.
 * The new vnode is returned locked.
 *
 * (um) points to the union mount structure for access to the
 * the mounting process's credentials.
 * (dvp) is the directory in which to create the shadow directory.
 * it is unlocked on entry and exit.
 * (cnp) is the componentname to be created.
 * (vpp) is the returned newly created shadow directory, which
 * is returned locked.
 *
 * N.B. We still attempt to create shadow directories even if the union
 * is mounted read-only, which is a little nonintuitive.
 */
int
union_mkshadow(struct union_mount *um, struct vnode *dvp,
	struct componentname *cnp, struct vnode **vpp)
{
	int error;
	struct vattr va;
	struct componentname cn;
	char *pnbuf;

	if (cnp->cn_namelen + 1 > MAXPATHLEN)
		return ENAMETOOLONG;
	pnbuf = PNBUF_GET();
	memcpy(pnbuf, cnp->cn_nameptr, cnp->cn_namelen);
	pnbuf[cnp->cn_namelen] = '\0';

	vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);

	error = union_do_lookup(dvp, &cn,
	    (um->um_op == UNMNT_ABOVE ? cnp->cn_cred : um->um_cred), pnbuf);
	if (error) {
		VOP_UNLOCK(dvp);
		PNBUF_PUT(pnbuf);
		return error;
	}

	/*
	 * policy: when creating the shadow directory in the
	 * upper layer, create it owned by the user who did
	 * the mount, group from parent directory, and mode
	 * 777 modified by umask (ie mostly identical to the
	 * mkdir syscall).  (jsp, kb)
	 */

	vattr_null(&va);
	va.va_type = VDIR;
	va.va_mode = um->um_cmode;

	KASSERT(*vpp == NULL);
	error = VOP_MKDIR(dvp, vpp, &cn, &va);
	VOP_UNLOCK(dvp);
	PNBUF_PUT(pnbuf);
	return error;
}

/*
 * Create a whiteout entry in the upper layer.
 *
 * (um) points to the union mount structure for access to the
 * the mounting process's credentials.
 * (dvp) is the directory in which to create the whiteout.
 * it is locked on entry and exit.
 * (cnp) is the componentname to be created.
 * (un) holds the path and its hash to be created.
 */
int
union_mkwhiteout(struct union_mount *um, struct vnode *dvp,
	struct componentname *cnp, struct union_node *un)
{
	int error;
	struct componentname cn;

	error = union_do_lookup(dvp, &cn,
	    (um->um_op == UNMNT_ABOVE ? cnp->cn_cred : um->um_cred),
	    un->un_path);
	if (error)
		return error;

	error = VOP_WHITEOUT(dvp, &cn, CREATE);
	return error;
}

/*
 * union_vn_create: creates and opens a new shadow file
 * on the upper union layer.  this function is similar
 * in spirit to calling vn_open but it avoids calling namei().
 * the problem with calling namei is that a) it locks too many
 * things, and b) it doesn't start at the "right" directory,
 * whereas union_do_lookup is told where to start.
 */
int
union_vn_create(struct vnode **vpp, struct union_node *un, struct lwp *l)
{
	struct vnode *vp;
	kauth_cred_t cred = l->l_cred;
	struct vattr vat;
	struct vattr *vap = &vat;
	int fmode = FFLAGS(O_WRONLY|O_CREAT|O_TRUNC|O_EXCL);
	int error;
	int cmode = UN_FILEMODE & ~l->l_proc->p_cwdi->cwdi_cmask;
	struct componentname cn;

	*vpp = NULLVP;

	vn_lock(un->un_dirvp, LK_EXCLUSIVE | LK_RETRY);

	error = union_do_lookup(un->un_dirvp, &cn, l->l_cred,
	    un->un_path);
	if (error) {
		VOP_UNLOCK(un->un_dirvp);
		return error;
	}

	/*
	 * Good - there was no race to create the file
	 * so go ahead and create it.  The permissions
	 * on the file will be 0666 modified by the
	 * current user's umask.  Access to the file, while
	 * it is unioned, will require access to the top *and*
	 * bottom files.  Access when not unioned will simply
	 * require access to the top-level file.
	 * TODO: confirm choice of access permissions.
	 */
	vattr_null(vap);
	vap->va_type = VREG;
	vap->va_mode = cmode;
	vp = NULL;
	error = VOP_CREATE(un->un_dirvp, &vp, &cn, vap);
	if (error) {
		VOP_UNLOCK(un->un_dirvp);
		return error;
	}

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_UNLOCK(un->un_dirvp);
	error = VOP_OPEN(vp, fmode, cred);
	if (error) {
		vput(vp);
		return error;
	}

	vp->v_writecount++;
	*vpp = vp;
	return 0;
}

int
union_vn_close(struct vnode *vp, int fmode, kauth_cred_t cred, struct lwp *l)
{

	if (fmode & FWRITE)
		--vp->v_writecount;
	return (VOP_CLOSE(vp, fmode, cred));
}

void
union_removed_upper(struct union_node *un)
{
	struct vnode *vp = UNIONTOV(un);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
#if 1
	/*
	 * We do not set the uppervp to NULLVP here, because lowervp
	 * may also be NULLVP, so this routine would end up creating
	 * a bogus union node with no upper or lower VP (that causes
	 * pain in many places that assume at least one VP exists).
	 * Since we've removed this node from the cache hash chains,
	 * it won't be found again.  When all current holders
	 * release it, union_inactive() will vgone() it.
	 */
	union_diruncache(un);
#else
	union_newupper(un, NULLVP);
#endif

	VOP_UNLOCK(vp);

	mutex_enter(&uhash_lock);
	if (un->un_cflags & UN_CACHED) {
		un->un_cflags &= ~UN_CACHED;
		LIST_REMOVE(un, un_cache);
	}
	mutex_exit(&uhash_lock);
}

#if 0
struct vnode *
union_lowervp(struct vnode *vp)
{
	struct union_node *un = VTOUNION(vp);

	if ((un->un_lowervp != NULLVP) &&
	    (vp->v_type == un->un_lowervp->v_type)) {
		if (vget(un->un_lowervp, 0, true /* wait */) == 0)
			return (un->un_lowervp);
	}

	return (NULLVP);
}
#endif

/*
 * determine whether a whiteout is needed
 * during a remove/rmdir operation.
 */
int
union_dowhiteout(struct union_node *un, kauth_cred_t cred)
{
	struct vattr va;

	if (un->un_lowervp != NULLVP)
		return (1);

	if (VOP_GETATTR(un->un_uppervp, &va, cred) == 0 &&
	    (va.va_flags & OPAQUE))
		return (1);

	return (0);
}

static void
union_dircache_r(struct vnode *vp, struct vnode ***vppp, int *cntp)
{
	struct union_node *un;

	if (vp->v_op != union_vnodeop_p) {
		if (vppp) {
			vref(vp);
			*(*vppp)++ = vp;
			if (--(*cntp) == 0)
				panic("union: dircache table too small");
		} else {
			(*cntp)++;
		}

		return;
	}

	un = VTOUNION(vp);
	if (un->un_uppervp != NULLVP)
		union_dircache_r(un->un_uppervp, vppp, cntp);
	if (un->un_lowervp != NULLVP)
		union_dircache_r(un->un_lowervp, vppp, cntp);
}

struct vnode *
union_dircache(struct vnode *vp, struct lwp *l)
{
	int cnt;
	struct vnode *nvp = NULLVP;
	struct vnode **vpp;
	struct vnode **dircache;
	int error;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	dircache = VTOUNION(vp)->un_dircache;

	nvp = NULLVP;

	if (dircache == 0) {
		cnt = 0;
		union_dircache_r(vp, 0, &cnt);
		cnt++;
		dircache = (struct vnode **)
				malloc(cnt * sizeof(struct vnode *),
					M_TEMP, M_WAITOK);
		vpp = dircache;
		union_dircache_r(vp, &vpp, &cnt);
		VTOUNION(vp)->un_dircache = dircache;
		*vpp = NULLVP;
		vpp = dircache + 1;
	} else {
		vpp = dircache;
		do {
			if (*vpp++ == VTOUNION(vp)->un_uppervp)
				break;
		} while (*vpp != NULLVP);
	}

	if (*vpp == NULLVP)
		goto out;

	vref(*vpp);
	error = union_allocvp(&nvp, vp->v_mount, NULLVP, NULLVP, 0, *vpp, NULLVP, 0);
	if (!error) {
		vn_lock(nvp, LK_EXCLUSIVE | LK_RETRY);
		VTOUNION(vp)->un_dircache = 0;
		VTOUNION(nvp)->un_dircache = dircache;
	}

out:
	VOP_UNLOCK(vp);
	return (nvp);
}

void
union_diruncache(struct union_node *un)
{
	struct vnode **vpp;

	KASSERT(VOP_ISLOCKED(UNIONTOV(un)) == LK_EXCLUSIVE);
	if (un->un_dircache != 0) {
		for (vpp = un->un_dircache; *vpp != NULLVP; vpp++)
			vrele(*vpp);
		free(un->un_dircache, M_TEMP);
		un->un_dircache = 0;
	}
}

/*
 * Check whether node can rmdir (check empty).
 */
int
union_check_rmdir(struct union_node *un, kauth_cred_t cred)
{
	int dirlen, eofflag, error;
	char *dirbuf;
	struct vattr va;
	struct vnode *tvp;
	struct dirent *dp, *edp;
	struct componentname cn;
	struct iovec aiov;
	struct uio auio;

	KASSERT(un->un_uppervp != NULL);

	/* Check upper for being opaque. */
	KASSERT(VOP_ISLOCKED(un->un_uppervp));
	error = VOP_GETATTR(un->un_uppervp, &va, cred);
	if (error || (va.va_flags & OPAQUE))
		return error;

	if (un->un_lowervp == NULL)
		return 0;

	/* Check lower for being empty. */
	vn_lock(un->un_lowervp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(un->un_lowervp, &va, cred);
	if (error) {
		VOP_UNLOCK(un->un_lowervp);
		return error;
	}
	dirlen = va.va_blocksize;
	dirbuf = kmem_alloc(dirlen, KM_SLEEP);
	if (dirbuf == NULL) {
		VOP_UNLOCK(un->un_lowervp);
		return ENOMEM;
	}
	/* error = 0; */
	eofflag = 0;
	auio.uio_offset = 0;
	do {
		aiov.iov_len = dirlen;
		aiov.iov_base = dirbuf;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = aiov.iov_len;
		auio.uio_rw = UIO_READ;
		UIO_SETUP_SYSSPACE(&auio);
		error = VOP_READDIR(un->un_lowervp, &auio, cred, &eofflag,
		    NULL, NULL);
		if (error)
			break;
		edp = (struct dirent *)&dirbuf[dirlen - auio.uio_resid];
		for (dp = (struct dirent *)dirbuf;
		    error == 0 && dp < edp;
		    dp = (struct dirent *)((char *)dp + dp->d_reclen)) {
			if (dp->d_reclen == 0) {
				error = ENOTEMPTY;
				break;
			}
			if (dp->d_type == DT_WHT ||
			    (dp->d_namlen == 1 && dp->d_name[0] == '.') ||
			    (dp->d_namlen == 2 && !memcmp(dp->d_name, "..", 2)))
				continue;
			/* Check for presence in the upper layer. */
			cn.cn_nameiop = LOOKUP;
			cn.cn_flags = ISLASTCN | RDONLY;
			cn.cn_cred = cred;
			cn.cn_nameptr = dp->d_name;
			cn.cn_namelen = dp->d_namlen;
			error = VOP_LOOKUP(un->un_uppervp, &tvp, &cn);
			if (error == ENOENT && (cn.cn_flags & ISWHITEOUT)) {
				error = 0;
				continue;
			}
			if (error == 0)
				vrele(tvp);
			error = ENOTEMPTY;
		}
	} while (error == 0 && !eofflag);
	kmem_free(dirbuf, dirlen);
	VOP_UNLOCK(un->un_lowervp);

	return error;
}

/*
 * This hook is called from vn_readdir() to switch to lower directory
 * entry after the upper directory is read.
 */
int
union_readdirhook(struct vnode **vpp, struct file *fp, struct lwp *l)
{
	struct vnode *vp = *vpp, *lvp;
	struct vattr va;
	int error;

	if (vp->v_op != union_vnodeop_p)
		return (0);

	/*
	 * If the directory is opaque,
	 * then don't show lower entries
	 */
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &va, fp->f_cred);
	VOP_UNLOCK(vp);
	if (error || (va.va_flags & OPAQUE))
		return error;

	if ((lvp = union_dircache(vp, l)) == NULLVP)
		return (0);

	error = VOP_OPEN(lvp, FREAD, fp->f_cred);
	if (error) {
		vput(lvp);
		return (error);
	}
	VOP_UNLOCK(lvp);
	fp->f_vnode = lvp;
	fp->f_offset = 0;
	error = vn_close(vp, FREAD, fp->f_cred);
	if (error)
		return (error);
	*vpp = lvp;
	return (0);
}
