/*	$NetBSD: msdosfs_denode.c,v 1.51 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (C) 1994, 1995, 1997 Wolfgang Solfrank.
 * Copyright (C) 1994, 1995, 1997 TooLs GmbH.
 * All rights reserved.
 * Original code by Paul Popelka (paulp@uts.amdahl.com) (see below).
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: msdosfs_denode.c,v 1.51 2015/03/28 19:24:05 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/fstrans.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>		/* defines "time" */
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>

#include <fs/msdosfs/bpb.h>
#include <fs/msdosfs/msdosfsmount.h>
#include <fs/msdosfs/direntry.h>
#include <fs/msdosfs/denode.h>
#include <fs/msdosfs/fat.h>

struct pool msdosfs_denode_pool;

extern int prtactive;

struct fh_key {
	struct msdosfsmount *fhk_mount;
	uint32_t fhk_dircluster;
	uint32_t fhk_diroffset;
};
struct fh_node {
	struct rb_node fh_rbnode;
	struct fh_key fh_key;
#define fh_mount	fh_key.fhk_mount
#define fh_dircluster	fh_key.fhk_dircluster
#define fh_diroffset	fh_key.fhk_diroffset
	uint32_t fh_gen;
};

static int
fh_compare_node_fh(void *ctx, const void *b, const void *key)
{
	const struct fh_node * const pnp = b;
	const struct fh_key * const fhp = key;

	/* msdosfs_fh_destroy() below depends on first sorting on fh_mount. */
	if (pnp->fh_mount != fhp->fhk_mount)
		return (intptr_t)pnp->fh_mount - (intptr_t)fhp->fhk_mount;
	if (pnp->fh_dircluster != fhp->fhk_dircluster)
		return pnp->fh_dircluster - fhp->fhk_dircluster;
	return pnp->fh_diroffset - fhp->fhk_diroffset;
}

static int
fh_compare_nodes(void *ctx, const void *parent, const void *node)
{
	const struct fh_node * const np = node;

	return fh_compare_node_fh(ctx, parent, &np->fh_key);
}

static uint32_t fh_generation;
static kmutex_t fh_lock;
static struct pool fh_pool;
static rb_tree_t fh_rbtree;
static const rb_tree_ops_t fh_rbtree_ops = {
	.rbto_compare_nodes = fh_compare_nodes,
	.rbto_compare_key = fh_compare_node_fh,
	.rbto_node_offset = offsetof(struct fh_node, fh_rbnode),
	.rbto_context = NULL
};

static const struct genfs_ops msdosfs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = msdosfs_gop_alloc,
	.gop_write = genfs_gop_write,
	.gop_markupdate = msdosfs_gop_markupdate,
};

MALLOC_DECLARE(M_MSDOSFSFAT);

void
msdosfs_init(void)
{

	malloc_type_attach(M_MSDOSFSMNT);
	malloc_type_attach(M_MSDOSFSFAT);
	malloc_type_attach(M_MSDOSFSTMP);
	pool_init(&msdosfs_denode_pool, sizeof(struct denode), 0, 0, 0,
	    "msdosnopl", &pool_allocator_nointr, IPL_NONE);
	pool_init(&fh_pool, sizeof(struct fh_node), 0, 0, 0,
	    "msdosfhpl", &pool_allocator_nointr, IPL_NONE);
	rb_tree_init(&fh_rbtree, &fh_rbtree_ops);
	mutex_init(&fh_lock, MUTEX_DEFAULT, IPL_NONE);
}

/*
 * Reinitialize.
 */

void
msdosfs_reinit(void)
{

}

void
msdosfs_done(void)
{
	pool_destroy(&msdosfs_denode_pool);
	pool_destroy(&fh_pool);
	mutex_destroy(&fh_lock);
	malloc_type_detach(M_MSDOSFSTMP);
	malloc_type_detach(M_MSDOSFSFAT);
	malloc_type_detach(M_MSDOSFSMNT);
}

/*
 * If deget() succeeds it returns with the gotten denode unlocked.
 *
 * pmp	     - address of msdosfsmount structure of the filesystem containing
 *	       the denode of interest.  The pm_dev field and the address of
 *	       the msdosfsmount structure are used.
 * dirclust  - which cluster bp contains, if dirclust is 0 (root directory)
 *	       diroffset is relative to the beginning of the root directory,
 *	       otherwise it is cluster relative.
 * diroffset - offset past begin of cluster of denode we want
 * vpp	     - returns the address of the gotten vnode.
 */
int
deget(struct msdosfsmount *pmp, u_long dirclust, u_long diroffset,
    struct vnode **vpp)
	/* pmp:	 so we know the maj/min number */
	/* dirclust:		 cluster this dir entry came from */
	/* diroffset:		 index of entry within the cluster */
	/* vpp:			 returns the addr of the gotten vnode */
{
	int error;
	struct denode_key key;

	/*
	 * On FAT32 filesystems, root is a (more or less) normal
	 * directory
	 */
	if (FAT32(pmp) && dirclust == MSDOSFSROOT)
		dirclust = pmp->pm_rootdirblk;

	memset(&key, 0, sizeof(key));
	key.dk_dirclust = dirclust;
	key.dk_diroffset = diroffset;
	/* key.dk_dirgen = NULL; */

	error = vcache_get(pmp->pm_mountp, &key, sizeof(key), vpp);
	return error;
}

int
msdosfs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	bool is_root;
	int error;
	extern int (**msdosfs_vnodeop_p)(void *);
	struct msdosfsmount *pmp;
	struct direntry *direntptr;
	struct denode *ldep;
	struct buf *bp;
	struct denode_key dkey;

	KASSERT(key_len == sizeof(dkey));
	memcpy(&dkey, key, key_len);
	KASSERT(dkey.dk_dirgen == NULL);

	pmp = VFSTOMSDOSFS(mp);
	is_root = ((dkey.dk_dirclust == MSDOSFSROOT ||
	    (FAT32(pmp) && dkey.dk_dirclust == pmp->pm_rootdirblk)) &&
	    dkey.dk_diroffset == MSDOSFSROOT_OFS);

#ifdef MSDOSFS_DEBUG
	printf("loadvnode(pmp %p, dirclust %lu, diroffset %lx, vp %p)\n",
	    pmp, dkey.dk_dirclust, dkey.dk_diroffset, vp);
#endif

	ldep = pool_get(&msdosfs_denode_pool, PR_WAITOK);
	memset(ldep, 0, sizeof *ldep);
	/* ldep->de_flag = 0; */
	/* ldep->de_devvp = 0; */
	/* ldep->de_lockf = 0; */
	ldep->de_dev = pmp->pm_dev;
	ldep->de_dirclust = dkey.dk_dirclust;
	ldep->de_diroffset = dkey.dk_diroffset;
	ldep->de_pmp = pmp;
	ldep->de_devvp = pmp->pm_devvp;
	ldep->de_refcnt = 1;
	fc_purge(ldep, 0);	/* init the FAT cache for this denode */

	/*
	 * Copy the directory entry into the denode area of the vnode.
	 */
	if (is_root) {
		/*
		 * Directory entry for the root directory. There isn't one,
		 * so we manufacture one. We should probably rummage
		 * through the root directory and find a label entry (if it
		 * exists), and then use the time and date from that entry
		 * as the time and date for the root denode.
		 */
		ldep->de_Attributes = ATTR_DIRECTORY;
		if (FAT32(pmp))
			ldep->de_StartCluster = pmp->pm_rootdirblk;
			/* de_FileSize will be filled in further down */
		else {
			ldep->de_StartCluster = MSDOSFSROOT;
			ldep->de_FileSize = pmp->pm_rootdirsize *
			    pmp->pm_BytesPerSec;
		}
		/*
		 * fill in time and date so that dos2unixtime() doesn't
		 * spit up when called from msdosfs_getattr() with root
		 * denode
		 */
		ldep->de_CHun = 0;
		ldep->de_CTime = 0x0000;	/* 00:00:00	 */
		ldep->de_CDate = (0 << DD_YEAR_SHIFT) | (1 << DD_MONTH_SHIFT)
		    | (1 << DD_DAY_SHIFT);
		/* Jan 1, 1980	 */
		ldep->de_ADate = ldep->de_CDate;
		ldep->de_MTime = ldep->de_CTime;
		ldep->de_MDate = ldep->de_CDate;
		/* leave the other fields as garbage */
	} else {
		error = readep(pmp, ldep->de_dirclust, ldep->de_diroffset,
		    &bp, &direntptr);
		if (error) {
			pool_put(&msdosfs_denode_pool, ldep);
			return error;
		}
		DE_INTERNALIZE(ldep, direntptr);
		brelse(bp, 0);
	}

	/*
	 * Fill in a few fields of the vnode and finish filling in the
	 * denode.
	 */
	if (ldep->de_Attributes & ATTR_DIRECTORY) {
		/*
		 * Since DOS directory entries that describe directories
		 * have 0 in the filesize field, we take this opportunity
		 * to find out the length of the directory and plug it into
		 * the denode structure.
		 */
		u_long size;

		vp->v_type = VDIR;
		if (ldep->de_StartCluster != MSDOSFSROOT) {
			error = pcbmap(ldep, CLUST_END, 0, &size, 0);
			if (error == E2BIG) {
				ldep->de_FileSize = de_cn2off(pmp, size);
				error = 0;
			} else
				printf("loadvnode(): pcbmap returned %d\n",
				    error);
		}
	} else
		vp->v_type = VREG;
	vref(ldep->de_devvp);
	if (is_root)
		vp->v_vflag |= VV_ROOT;
	vp->v_tag = VT_MSDOSFS;
	vp->v_op = msdosfs_vnodeop_p;
	vp->v_data = ldep;
	ldep->de_vnode = vp;
	genfs_node_init(vp, &msdosfs_genfsops);
	uvm_vnp_setsize(vp, ldep->de_FileSize);
	*new_key = &ldep->de_key;

	return 0;
}

int
deupdat(struct denode *dep, int waitfor)
{

	return (msdosfs_update(DETOV(dep), NULL, NULL,
	    waitfor ? UPDATE_WAIT : 0));
}

/*
 * Truncate the file described by dep to the length specified by length.
 */
int
detrunc(struct denode *dep, u_long length, int flags, kauth_cred_t cred)
{
	int error;
	int allerror;
	u_long eofentry;
	u_long chaintofree = 0;
	daddr_t bn, lastblock;
	int boff;
	int isadir = dep->de_Attributes & ATTR_DIRECTORY;
	struct buf *bp;
	struct msdosfsmount *pmp = dep->de_pmp;

#ifdef MSDOSFS_DEBUG
	printf("detrunc(): file %s, length %lu, flags %x\n", dep->de_Name, length, flags);
#endif

	/*
	 * Disallow attempts to truncate the root directory since it is of
	 * fixed size.  That's just the way dos filesystems are.  We use
	 * the VROOT bit in the vnode because checking for the directory
	 * bit and a startcluster of 0 in the denode is not adequate to
	 * recognize the root directory at this point in a file or
	 * directory's life.
	 */
	if ((DETOV(dep)->v_vflag & VV_ROOT) && !FAT32(pmp)) {
		printf("detrunc(): can't truncate root directory, clust %ld, offset %ld\n",
		    dep->de_dirclust, dep->de_diroffset);
		return (EINVAL);
	}

	uvm_vnp_setsize(DETOV(dep), length);

	if (dep->de_FileSize < length)
		return (deextend(dep, length, cred));
	lastblock = de_clcount(pmp, length) - 1;

	/*
	 * If the desired length is 0 then remember the starting cluster of
	 * the file and set the StartCluster field in the directory entry
	 * to 0.  If the desired length is not zero, then get the number of
	 * the last cluster in the shortened file.  Then get the number of
	 * the first cluster in the part of the file that is to be freed.
	 * Then set the next cluster pointer in the last cluster of the
	 * file to CLUST_EOFE.
	 */
	if (length == 0) {
		chaintofree = dep->de_StartCluster;
		dep->de_StartCluster = 0;
		eofentry = ~0;
	} else {
		error = pcbmap(dep, lastblock, 0, &eofentry, 0);
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): pcbmap fails %d\n", error);
#endif
			return (error);
		}
	}

	/*
	 * If the new length is not a multiple of the cluster size then we
	 * must zero the tail end of the new last cluster in case it
	 * becomes part of the file again because of a seek.
	 */
	if ((boff = length & pmp->pm_crbomask) != 0) {
		if (isadir) {
			bn = cntobn(pmp, eofentry);
			error = bread(pmp->pm_devvp, de_bn2kb(pmp, bn),
			    pmp->pm_bpcluster, B_MODIFY, &bp);
			if (error) {
#ifdef MSDOSFS_DEBUG
				printf("detrunc(): bread fails %d\n", error);
#endif
				return (error);
			}
			memset((char *)bp->b_data + boff, 0,
			    pmp->pm_bpcluster - boff);
			if (flags & IO_SYNC)
				bwrite(bp);
			else
				bdwrite(bp);
		} else {
			ubc_zerorange(&DETOV(dep)->v_uobj, length,
				      pmp->pm_bpcluster - boff,
				      UBC_UNMAP_FLAG(DETOV(dep)));
		}
	}

	/*
	 * Write out the updated directory entry.  Even if the update fails
	 * we free the trailing clusters.
	 */
	dep->de_FileSize = length;
	if (!isadir)
		dep->de_flag |= DE_UPDATE|DE_MODIFIED;
	vtruncbuf(DETOV(dep), lastblock + 1, 0, 0);
	allerror = deupdat(dep, 1);
#ifdef MSDOSFS_DEBUG
	printf("detrunc(): allerror %d, eofentry %lu\n",
	       allerror, eofentry);
#endif

	fc_purge(dep, lastblock + 1);

	/*
	 * If we need to break the cluster chain for the file then do it
	 * now.
	 */
	if (eofentry != ~0) {
		error = fatentry(FAT_GET_AND_SET, pmp, eofentry,
				 &chaintofree, CLUST_EOFE);
		if (error) {
#ifdef MSDOSFS_DEBUG
			printf("detrunc(): fatentry errors %d\n", error);
#endif
			return (error);
		}
		fc_setcache(dep, FC_LASTFC, de_cluster(pmp, length - 1),
			    eofentry);
	}

	/*
	 * Now free the clusters removed from the file because of the
	 * truncation.
	 */
	if (chaintofree != 0 && !MSDOSFSEOF(chaintofree, pmp->pm_fatmask))
		freeclusterchain(pmp, chaintofree);

	return (allerror);
}

/*
 * Extend the file described by dep to length specified by length.
 */
int
deextend(struct denode *dep, u_long length, kauth_cred_t cred)
{
	struct msdosfsmount *pmp = dep->de_pmp;
	u_long count, osize;
	int error;

	/*
	 * The root of a DOS filesystem cannot be extended.
	 */
	if ((DETOV(dep)->v_vflag & VV_ROOT) && !FAT32(pmp))
		return (EINVAL);

	/*
	 * Directories cannot be extended.
	 */
	if (dep->de_Attributes & ATTR_DIRECTORY)
		return (EISDIR);

	if (length <= dep->de_FileSize)
		panic("deextend: file too large");

	/*
	 * Compute the number of clusters to allocate.
	 */
	count = de_clcount(pmp, length) - de_clcount(pmp, dep->de_FileSize);
	if (count > 0) {
		if (count > pmp->pm_freeclustercount)
			return (ENOSPC);
		error = extendfile(dep, count, NULL, NULL, DE_CLEAR);
		if (error) {
			/* truncate the added clusters away again */
			(void) detrunc(dep, dep->de_FileSize, 0, cred);
			return (error);
		}
	}

	/*
	 * Zero extend file range; ubc_zerorange() uses ubc_alloc() and a
	 * memset(); we set the write size so ubc won't read in file data that
	 * is zero'd later.
	 */
	osize = dep->de_FileSize;
	dep->de_FileSize = length;
	uvm_vnp_setwritesize(DETOV(dep), (voff_t)dep->de_FileSize);
	dep->de_flag |= DE_UPDATE|DE_MODIFIED;
	ubc_zerorange(&DETOV(dep)->v_uobj, (off_t)osize,
	    (size_t)(round_page(dep->de_FileSize) - osize),
	    UBC_UNMAP_FLAG(DETOV(dep)));
	uvm_vnp_setsize(DETOV(dep), (voff_t)dep->de_FileSize);
	return (deupdat(dep, 1));
}

int
msdosfs_reclaim(void *v)
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;
	struct denode *dep = VTODE(vp);

	fstrans_start(mp, FSTRANS_LAZY);
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_reclaim(): dep %p, file %s, refcnt %ld\n",
	    dep, dep->de_Name, dep->de_refcnt);
#endif

	if (prtactive && vp->v_usecount > 1)
		vprint("msdosfs_reclaim(): pushing active", vp);
	/*
	 * Remove the denode from the vnode cache.
	 */
	vcache_remove(vp->v_mount, &dep->de_key, sizeof(dep->de_key));
	/*
	 * Purge old data structures associated with the denode.
	 */
	if (dep->de_devvp) {
		vrele(dep->de_devvp);
		dep->de_devvp = 0;
	}
#if 0 /* XXX */
	dep->de_flag = 0;
#endif
	/*
	 * To interlock with msdosfs_sync().
	 */
	genfs_node_destroy(vp);
	mutex_enter(vp->v_interlock);
	vp->v_data = NULL;
	mutex_exit(vp->v_interlock);
	pool_put(&msdosfs_denode_pool, dep);
	fstrans_done(mp);
	return (0);
}

int
msdosfs_inactive(void *v)
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		bool *a_recycle;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mount *mp = vp->v_mount;
	struct denode *dep = VTODE(vp);
	int error = 0;

#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, de_Name[0] %x\n", dep, dep->de_Name[0]);
#endif

	fstrans_start(mp, FSTRANS_LAZY);
	/*
	 * Get rid of denodes related to stale file handles.
	 */
	if (dep->de_Name[0] == SLOT_DELETED)
		goto out;

	/*
	 * If the file has been deleted and it is on a read/write
	 * filesystem, then truncate the file, and mark the directory slot
	 * as empty.  (This may not be necessary for the dos filesystem.)
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): dep %p, refcnt %ld, mntflag %x %s\n",
	       dep, dep->de_refcnt, vp->v_mount->mnt_flag,
		(vp->v_mount->mnt_flag & MNT_RDONLY) ? "MNT_RDONLY" : "");
#endif
	if (dep->de_refcnt <= 0 && (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
		if (dep->de_FileSize != 0) {
			error = detrunc(dep, (u_long)0, 0, NOCRED);
		}
		dep->de_Name[0] = SLOT_DELETED;
		msdosfs_fh_remove(dep->de_pmp,
		    dep->de_dirclust, dep->de_diroffset);
	}
	deupdat(dep, 0);
out:
	/*
	 * If we are done with the denode, reclaim it
	 * so that it can be reused immediately.
	 */
#ifdef MSDOSFS_DEBUG
	printf("msdosfs_inactive(): v_usecount %d, de_Name[0] %x\n",
		vp->v_usecount, dep->de_Name[0]);
#endif
	*ap->a_recycle = (dep->de_Name[0] == SLOT_DELETED);
	VOP_UNLOCK(vp);
	fstrans_done(mp);
	return (error);
}

int
msdosfs_gop_alloc(struct vnode *vp, off_t off,
    off_t len, int flags, kauth_cred_t cred)
{
	return 0;
}

void
msdosfs_gop_markupdate(struct vnode *vp, int flags)
{
	u_long mask = 0;

	if ((flags & GOP_UPDATE_ACCESSED) != 0) {
		mask = DE_ACCESS;
	}
	if ((flags & GOP_UPDATE_MODIFIED) != 0) {
		mask |= DE_UPDATE;
	}
	if (mask) {
		struct denode *dep = VTODE(vp);

		dep->de_flag |= mask;
	}
}

int
msdosfs_fh_enter(struct msdosfsmount *pmp,
     uint32_t dircluster, uint32_t diroffset, uint32_t *genp)
{
	struct fh_key fhkey;
	struct fh_node *fhp;

	fhkey.fhk_mount = pmp;
	fhkey.fhk_dircluster = dircluster;
	fhkey.fhk_diroffset = diroffset;

	mutex_enter(&fh_lock);
	fhp = rb_tree_find_node(&fh_rbtree, &fhkey);
	if (fhp == NULL) {
		mutex_exit(&fh_lock);
		fhp = pool_get(&fh_pool, PR_WAITOK);
		mutex_enter(&fh_lock);
		fhp->fh_key = fhkey;
		fhp->fh_gen = fh_generation++;
		rb_tree_insert_node(&fh_rbtree, fhp);
	}
	*genp = fhp->fh_gen;
	mutex_exit(&fh_lock);
	return 0;
}

int
msdosfs_fh_remove(struct msdosfsmount *pmp,
     uint32_t dircluster, uint32_t diroffset)
{
	struct fh_key fhkey;
	struct fh_node *fhp;

	fhkey.fhk_mount = pmp;
	fhkey.fhk_dircluster = dircluster;
	fhkey.fhk_diroffset = diroffset;

	mutex_enter(&fh_lock);
	fhp = rb_tree_find_node(&fh_rbtree, &fhkey);
	if (fhp == NULL) {
		mutex_exit(&fh_lock);
		return ENOENT;
	}
	rb_tree_remove_node(&fh_rbtree, fhp);
	mutex_exit(&fh_lock);
	pool_put(&fh_pool, fhp);
	return 0;
}

int
msdosfs_fh_lookup(struct msdosfsmount *pmp,
     uint32_t dircluster, uint32_t diroffset, uint32_t *genp)
{
	struct fh_key fhkey;
	struct fh_node *fhp;

	fhkey.fhk_mount = pmp;
	fhkey.fhk_dircluster = dircluster;
	fhkey.fhk_diroffset = diroffset;

	mutex_enter(&fh_lock);
	fhp = rb_tree_find_node(&fh_rbtree, &fhkey);
	if (fhp == NULL) {
		mutex_exit(&fh_lock);
		return ESTALE;
	}
	*genp = fhp->fh_gen;
	mutex_exit(&fh_lock);
	return 0;
}

void
msdosfs_fh_destroy(struct msdosfsmount *pmp)
{
	struct fh_key fhkey;
	struct fh_node *fhp, *nfhp;

	fhkey.fhk_mount = pmp;
	fhkey.fhk_dircluster = 0;
	fhkey.fhk_diroffset = 0;

	mutex_enter(&fh_lock);
	for (fhp = rb_tree_find_node_geq(&fh_rbtree, &fhkey);
	    fhp != NULL && fhp->fh_mount == pmp; fhp = nfhp) {
		nfhp = rb_tree_iterate(&fh_rbtree, fhp, RB_DIR_RIGHT);
		rb_tree_remove_node(&fh_rbtree, fhp);
		pool_put(&fh_pool, fhp);
	}
#ifdef DIAGNOSTIC
	RB_TREE_FOREACH(fhp, &fh_rbtree) {
		KASSERT(fhp->fh_mount != pmp);
	}
#endif
	mutex_exit(&fh_lock);
}
