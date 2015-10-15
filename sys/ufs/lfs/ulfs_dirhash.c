/*	$NetBSD: ulfs_dirhash.c,v 1.14 2015/09/21 01:24:23 dholland Exp $	*/
/*  from NetBSD: ufs_dirhash.c,v 1.34 2009/10/05 23:48:08 rmind Exp  */

/*
 * Copyright (c) 2001, 2002 Ian Dowse.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/ufs/ufs/ufs_dirhash.c,v 1.3.2.8 2004/12/08 11:54:13 dwmalone Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ulfs_dirhash.c,v 1.14 2015/09/21 01:24:23 dholland Exp $");

/*
 * This implements a hash-based lookup scheme for ULFS directories.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/types.h>
#include <sys/hash.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/sysctl.h>
#include <sys/atomic.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/ulfs_dirhash.h>
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_bswap.h>
#include <ufs/lfs/ulfs_extern.h>

#define WRAPINCR(val, limit)	(((val) + 1 == (limit)) ? 0 : ((val) + 1))
#define WRAPDECR(val, limit)	(((val) == 0) ? ((limit) - 1) : ((val) - 1))
#define OFSFMT(ip)		((ip)->i_lfs->um_maxsymlinklen <= 0)
#define BLKFREE2IDX(n)		((n) > DH_NFSTATS ? DH_NFSTATS : (n))

static u_int ulfs_dirhashminblks = 5;
static u_int ulfs_dirhashmaxmem = 2 * 1024 * 1024;
static u_int ulfs_dirhashmem;
static u_int ulfs_dirhashcheck = 0;

static int ulfsdirhash_hash(struct dirhash *dh, const char *name, int namelen);
static void ulfsdirhash_adjfree(struct dirhash *dh, doff_t offset, int diff,
	   int dirblksiz);
static void ulfsdirhash_delslot(struct dirhash *dh, int slot);
static int ulfsdirhash_findslot(struct dirhash *dh, const char *name,
	   int namelen, doff_t offset);
static doff_t ulfsdirhash_getprev(struct lfs *fs, LFS_DIRHEADER *dp,
	   doff_t offset, int dirblksiz);
static int ulfsdirhash_recycle(int wanted);

static pool_cache_t ulfsdirhashblk_cache;
static pool_cache_t ulfsdirhash_cache;

#define DIRHASHLIST_LOCK()		mutex_enter(&ulfsdirhash_lock)
#define DIRHASHLIST_UNLOCK()		mutex_exit(&ulfsdirhash_lock)
#define DIRHASH_LOCK(dh)		mutex_enter(&(dh)->dh_lock)
#define DIRHASH_UNLOCK(dh)		mutex_exit(&(dh)->dh_lock)
#define DIRHASH_BLKALLOC()		\
    pool_cache_get(ulfsdirhashblk_cache, PR_NOWAIT)
#define DIRHASH_BLKFREE(ptr)		\
    pool_cache_put(ulfsdirhashblk_cache, ptr)

/* Dirhash list; recently-used entries are near the tail. */
static TAILQ_HEAD(, dirhash) ulfsdirhash_list;

/* Protects: ulfsdirhash_list, `dh_list' field, ulfs_dirhashmem. */
static kmutex_t ulfsdirhash_lock;

static struct sysctllog *ulfsdirhash_sysctl_log;

/*
 * Locking order:
 *	ulfsdirhash_lock
 *	dh_lock
 *
 * The dh_lock mutex should be acquired either via the inode lock, or via
 * ulfsdirhash_lock. Only the owner of the inode may free the associated
 * dirhash, but anything can steal its memory and set dh_hash to NULL.
 */

/*
 * Attempt to build up a hash table for the directory contents in
 * inode 'ip'. Returns 0 on success, or -1 of the operation failed.
 */
int
ulfsdirhash_build(struct inode *ip)
{
	struct lfs *fs = ip->i_lfs;
	struct dirhash *dh;
	struct buf *bp = NULL;
	LFS_DIRHEADER *ep;
	struct vnode *vp;
	doff_t bmask, pos;
	int dirblocks, i, j, memreqd, nblocks, narrays, nslots, slot;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	/* Check if we can/should use dirhash. */
	if (ip->i_dirhash == NULL) {
		if (ip->i_size < (ulfs_dirhashminblks * dirblksiz) || OFSFMT(ip))
			return (-1);
	} else {
		/* Hash exists, but sysctls could have changed. */
		if (ip->i_size < (ulfs_dirhashminblks * dirblksiz) ||
		    ulfs_dirhashmem > ulfs_dirhashmaxmem) {
			ulfsdirhash_free(ip);
			return (-1);
		}
		/* Check if hash exists and is intact (note: unlocked read). */
		if (ip->i_dirhash->dh_hash != NULL)
			return (0);
		/* Free the old, recycled hash and build a new one. */
		ulfsdirhash_free(ip);
	}

	/* Don't hash removed directories. */
	if (ip->i_nlink == 0)
		return (-1);

	vp = ip->i_vnode;
	/* Allocate 50% more entries than this dir size could ever need. */
	KASSERT(ip->i_size >= dirblksiz);
	nslots = ip->i_size / LFS_DIRECTSIZ(fs, 1);
	nslots = (nslots * 3 + 1) / 2;
	narrays = howmany(nslots, DH_NBLKOFF);
	nslots = narrays * DH_NBLKOFF;
	dirblocks = howmany(ip->i_size, dirblksiz);
	nblocks = (dirblocks * 3 + 1) / 2;

	memreqd = sizeof(*dh) + narrays * sizeof(*dh->dh_hash) +
	    narrays * DH_NBLKOFF * sizeof(**dh->dh_hash) +
	    nblocks * sizeof(*dh->dh_blkfree);

	while (atomic_add_int_nv(&ulfs_dirhashmem, memreqd) >
	    ulfs_dirhashmaxmem) {
		atomic_add_int(&ulfs_dirhashmem, -memreqd);
		if (memreqd > ulfs_dirhashmaxmem / 2)
			return (-1);
		/* Try to free some space. */
		if (ulfsdirhash_recycle(memreqd) != 0)
			return (-1);
	        else
		    	DIRHASHLIST_UNLOCK();
	}

	/*
	 * Use non-blocking mallocs so that we will revert to a linear
	 * lookup on failure rather than potentially blocking forever.
	 */
	dh = pool_cache_get(ulfsdirhash_cache, PR_NOWAIT);
	if (dh == NULL) {
		atomic_add_int(&ulfs_dirhashmem, -memreqd);
		return (-1);
	}
	memset(dh, 0, sizeof(*dh));
	mutex_init(&dh->dh_lock, MUTEX_DEFAULT, IPL_NONE);
	DIRHASH_LOCK(dh);
	dh->dh_hashsz = narrays * sizeof(dh->dh_hash[0]);
	dh->dh_hash = kmem_zalloc(dh->dh_hashsz, KM_NOSLEEP);
	dh->dh_blkfreesz = nblocks * sizeof(dh->dh_blkfree[0]);
	dh->dh_blkfree = kmem_zalloc(dh->dh_blkfreesz, KM_NOSLEEP);
	if (dh->dh_hash == NULL || dh->dh_blkfree == NULL)
		goto fail;
	for (i = 0; i < narrays; i++) {
		if ((dh->dh_hash[i] = DIRHASH_BLKALLOC()) == NULL)
			goto fail;
		for (j = 0; j < DH_NBLKOFF; j++)
			dh->dh_hash[i][j] = DIRHASH_EMPTY;
	}

	/* Initialise the hash table and block statistics. */
	dh->dh_narrays = narrays;
	dh->dh_hlen = nslots;
	dh->dh_nblk = nblocks;
	dh->dh_dirblks = dirblocks;
	for (i = 0; i < dirblocks; i++)
		dh->dh_blkfree[i] = dirblksiz / DIRALIGN;
	for (i = 0; i < DH_NFSTATS; i++)
		dh->dh_firstfree[i] = -1;
	dh->dh_firstfree[DH_NFSTATS] = 0;
	dh->dh_seqopt = 0;
	dh->dh_seqoff = 0;
	dh->dh_score = DH_SCOREINIT;
	ip->i_dirhash = dh;

	bmask = VFSTOULFS(vp->v_mount)->um_mountp->mnt_stat.f_iosize - 1;
	pos = 0;
	while (pos < ip->i_size) {
		if ((curcpu()->ci_schedstate.spc_flags & SPCF_SHOULDYIELD)
		    != 0) {
			preempt();
		}
		/* If necessary, get the next directory block. */
		if ((pos & bmask) == 0) {
			if (bp != NULL)
				brelse(bp, 0);
			if (ulfs_blkatoff(vp, (off_t)pos, NULL, &bp, false) != 0)
				goto fail;
		}

		/* Add this entry to the hash. */
		ep = (LFS_DIRHEADER *)((char *)bp->b_data + (pos & bmask));
		if (lfs_dir_getreclen(fs, ep) == 0 || lfs_dir_getreclen(fs, ep) >
		    dirblksiz - (pos & (dirblksiz - 1))) {
			/* Corrupted directory. */
			brelse(bp, 0);
			goto fail;
		}
		if (lfs_dir_getino(fs, ep) != 0) {
			/* Add the entry (simplified ulfsdirhash_add). */
			slot = ulfsdirhash_hash(dh, lfs_dir_nameptr(fs, ep),
						lfs_dir_getnamlen(fs, ep));
			while (DH_ENTRY(dh, slot) != DIRHASH_EMPTY)
				slot = WRAPINCR(slot, dh->dh_hlen);
			dh->dh_hused++;
			DH_ENTRY(dh, slot) = pos;
			ulfsdirhash_adjfree(dh, pos, -LFS_DIRSIZ(fs, ep),
			    dirblksiz);
		}
		pos += lfs_dir_getreclen(fs, ep);
	}

	if (bp != NULL)
		brelse(bp, 0);
	DIRHASHLIST_LOCK();
	TAILQ_INSERT_TAIL(&ulfsdirhash_list, dh, dh_list);
	dh->dh_onlist = 1;
	DIRHASH_UNLOCK(dh);
	DIRHASHLIST_UNLOCK();
	return (0);

fail:
	DIRHASH_UNLOCK(dh);
	if (dh->dh_hash != NULL) {
		for (i = 0; i < narrays; i++)
			if (dh->dh_hash[i] != NULL)
				DIRHASH_BLKFREE(dh->dh_hash[i]);
		kmem_free(dh->dh_hash, dh->dh_hashsz);
	}
	if (dh->dh_blkfree != NULL)
		kmem_free(dh->dh_blkfree, dh->dh_blkfreesz);
	mutex_destroy(&dh->dh_lock);
	pool_cache_put(ulfsdirhash_cache, dh);
	ip->i_dirhash = NULL;
	atomic_add_int(&ulfs_dirhashmem, -memreqd);
	return (-1);
}

/*
 * Free any hash table associated with inode 'ip'.
 */
void
ulfsdirhash_free(struct inode *ip)
{
	struct dirhash *dh;
	int i, mem;

	if ((dh = ip->i_dirhash) == NULL)
		return;

	if (dh->dh_onlist) {
		DIRHASHLIST_LOCK();
		if (dh->dh_onlist)
			TAILQ_REMOVE(&ulfsdirhash_list, dh, dh_list);
		DIRHASHLIST_UNLOCK();
	}

	/* The dirhash pointed to by 'dh' is exclusively ours now. */
	mem = sizeof(*dh);
	if (dh->dh_hash != NULL) {
		for (i = 0; i < dh->dh_narrays; i++)
			DIRHASH_BLKFREE(dh->dh_hash[i]);
		kmem_free(dh->dh_hash, dh->dh_hashsz);
		kmem_free(dh->dh_blkfree, dh->dh_blkfreesz);
		mem += dh->dh_hashsz;
		mem += dh->dh_narrays * DH_NBLKOFF * sizeof(**dh->dh_hash);
		mem += dh->dh_nblk * sizeof(*dh->dh_blkfree);
	}
	mutex_destroy(&dh->dh_lock);
	pool_cache_put(ulfsdirhash_cache, dh);
	ip->i_dirhash = NULL;

	atomic_add_int(&ulfs_dirhashmem, -mem);
}

/*
 * Find the offset of the specified name within the given inode.
 * Returns 0 on success, ENOENT if the entry does not exist, or
 * EJUSTRETURN if the caller should revert to a linear search.
 *
 * If successful, the directory offset is stored in *offp, and a
 * pointer to a struct buf containing the entry is stored in *bpp. If
 * prevoffp is non-NULL, the offset of the previous entry within
 * the DIRBLKSIZ-sized block is stored in *prevoffp (if the entry
 * is the first in a block, the start of the block is used).
 */
int
ulfsdirhash_lookup(struct inode *ip, const char *name, int namelen, doff_t *offp,
    struct buf **bpp, doff_t *prevoffp)
{
	struct lfs *fs = ip->i_lfs;
	struct dirhash *dh, *dh_next;
	LFS_DIRHEADER *dp;
	struct vnode *vp;
	struct buf *bp;
	doff_t blkoff, bmask, offset, prevoff;
	int i, slot;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return (EJUSTRETURN);

	/*
	 * Move this dirhash towards the end of the list if it has a
	 * score higher than the next entry, and acquire the dh_lock.
	 * Optimise the case where it's already the last by performing
	 * an unlocked read of the TAILQ_NEXT pointer.
	 *
	 * In both cases, end up holding just dh_lock.
	 */
	if (TAILQ_NEXT(dh, dh_list) != NULL) {
		DIRHASHLIST_LOCK();
		DIRHASH_LOCK(dh);
		/*
		 * If the new score will be greater than that of the next
		 * entry, then move this entry past it. With both mutexes
		 * held, dh_next won't go away, but its dh_score could
		 * change; that's not important since it is just a hint.
		 */
		if (dh->dh_hash != NULL &&
		    (dh_next = TAILQ_NEXT(dh, dh_list)) != NULL &&
		    dh->dh_score >= dh_next->dh_score) {
			KASSERT(dh->dh_onlist);
			TAILQ_REMOVE(&ulfsdirhash_list, dh, dh_list);
			TAILQ_INSERT_AFTER(&ulfsdirhash_list, dh_next, dh,
			    dh_list);
		}
		DIRHASHLIST_UNLOCK();
	} else {
		/* Already the last, though that could change as we wait. */
		DIRHASH_LOCK(dh);
	}
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return (EJUSTRETURN);
	}

	/* Update the score. */
	if (dh->dh_score < DH_SCOREMAX)
		dh->dh_score++;

	vp = ip->i_vnode;
	bmask = VFSTOULFS(vp->v_mount)->um_mountp->mnt_stat.f_iosize - 1;
	blkoff = -1;
	bp = NULL;
restart:
	slot = ulfsdirhash_hash(dh, name, namelen);

	if (dh->dh_seqopt) {
		/*
		 * Sequential access optimisation. dh_seqoff contains the
		 * offset of the directory entry immediately following
		 * the last entry that was looked up. Check if this offset
		 * appears in the hash chain for the name we are looking for.
		 */
		for (i = slot; (offset = DH_ENTRY(dh, i)) != DIRHASH_EMPTY;
		    i = WRAPINCR(i, dh->dh_hlen))
			if (offset == dh->dh_seqoff)
				break;
		if (offset == dh->dh_seqoff) {
			/*
			 * We found an entry with the expected offset. This
			 * is probably the entry we want, but if not, the
			 * code below will turn off seqoff and retry.
			 */
			slot = i;
		} else
			dh->dh_seqopt = 0;
	}

	for (; (offset = DH_ENTRY(dh, slot)) != DIRHASH_EMPTY;
	    slot = WRAPINCR(slot, dh->dh_hlen)) {
		if (offset == DIRHASH_DEL)
			continue;

		if (offset < 0 || offset >= ip->i_size)
			panic("ulfsdirhash_lookup: bad offset in hash array");
		if ((offset & ~bmask) != blkoff) {
			if (bp != NULL)
				brelse(bp, 0);
			blkoff = offset & ~bmask;
			if (ulfs_blkatoff(vp, (off_t)blkoff,
			    NULL, &bp, false) != 0) {
				DIRHASH_UNLOCK(dh);
				return (EJUSTRETURN);
			}
		}
		dp = (LFS_DIRHEADER *)((char *)bp->b_data + (offset & bmask));
		if (lfs_dir_getreclen(fs, dp) == 0 || lfs_dir_getreclen(fs, dp) >
		    dirblksiz - (offset & (dirblksiz - 1))) {
			/* Corrupted directory. */
			DIRHASH_UNLOCK(dh);
			brelse(bp, 0);
			return (EJUSTRETURN);
		}
		if (lfs_dir_getnamlen(fs, dp) == namelen &&
		    memcmp(lfs_dir_nameptr(fs, dp), name, namelen) == 0) {
			/* Found. Get the prev offset if needed. */
			if (prevoffp != NULL) {
				if (offset & (dirblksiz - 1)) {
					prevoff = ulfsdirhash_getprev(fs, dp,
					    offset, dirblksiz);
					if (prevoff == -1) {
						brelse(bp, 0);
						return (EJUSTRETURN);
					}
				} else
					prevoff = offset;
				*prevoffp = prevoff;
			}

			/* Check for sequential access, and update offset. */
			if (dh->dh_seqopt == 0 && dh->dh_seqoff == offset)
				dh->dh_seqopt = 1;
			dh->dh_seqoff = offset + LFS_DIRSIZ(fs, dp);
			DIRHASH_UNLOCK(dh);

			*bpp = bp;
			*offp = offset;
			return (0);
		}

		if (dh->dh_hash == NULL) {
			DIRHASH_UNLOCK(dh);
			if (bp != NULL)
				brelse(bp, 0);
			ulfsdirhash_free(ip);
			return (EJUSTRETURN);
		}
		/*
		 * When the name doesn't match in the seqopt case, go back
		 * and search normally.
		 */
		if (dh->dh_seqopt) {
			dh->dh_seqopt = 0;
			goto restart;
		}
	}
	DIRHASH_UNLOCK(dh);
	if (bp != NULL)
		brelse(bp, 0);
	return (ENOENT);
}

/*
 * Find a directory block with room for 'slotneeded' bytes. Returns
 * the offset of the directory entry that begins the free space.
 * This will either be the offset of an existing entry that has free
 * space at the end, or the offset of an entry with d_ino == 0 at
 * the start of a DIRBLKSIZ block.
 *
 * To use the space, the caller may need to compact existing entries in
 * the directory. The total number of bytes in all of the entries involved
 * in the compaction is stored in *slotsize. In other words, all of
 * the entries that must be compacted are exactly contained in the
 * region beginning at the returned offset and spanning *slotsize bytes.
 *
 * Returns -1 if no space was found, indicating that the directory
 * must be extended.
 */
doff_t
ulfsdirhash_findfree(struct inode *ip, int slotneeded, int *slotsize)
{
	struct lfs *fs = ip->i_lfs;
	LFS_DIRHEADER *dp;
	struct dirhash *dh;
	struct buf *bp;
	doff_t pos, slotstart;
	int dirblock, error, freebytes, i;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return (-1);

	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return (-1);
	}

	/* Find a directory block with the desired free space. */
	dirblock = -1;
	for (i = howmany(slotneeded, DIRALIGN); i <= DH_NFSTATS; i++)
		if ((dirblock = dh->dh_firstfree[i]) != -1)
			break;
	if (dirblock == -1) {
		DIRHASH_UNLOCK(dh);
		return (-1);
	}

	KASSERT(dirblock < dh->dh_nblk &&
	    dh->dh_blkfree[dirblock] >= howmany(slotneeded, DIRALIGN));
	pos = dirblock * dirblksiz;
	error = ulfs_blkatoff(ip->i_vnode, (off_t)pos, (void *)&dp, &bp, false);
	if (error) {
		DIRHASH_UNLOCK(dh);
		return (-1);
	}
	/* Find the first entry with free space. */
	for (i = 0; i < dirblksiz; ) {
		if (lfs_dir_getreclen(fs, dp) == 0) {
			DIRHASH_UNLOCK(dh);
			brelse(bp, 0);
			return (-1);
		}
		if (lfs_dir_getino(fs, dp) == 0 || lfs_dir_getreclen(fs, dp) > LFS_DIRSIZ(fs, dp))
			break;
		i += lfs_dir_getreclen(fs, dp);
		dp = LFS_NEXTDIR(fs, dp);
	}
	if (i > dirblksiz) {
		DIRHASH_UNLOCK(dh);
		brelse(bp, 0);
		return (-1);
	}
	slotstart = pos + i;

	/* Find the range of entries needed to get enough space */
	freebytes = 0;
	while (i < dirblksiz && freebytes < slotneeded) {
		freebytes += lfs_dir_getreclen(fs, dp);
		if (lfs_dir_getino(fs, dp) != 0)
			freebytes -= LFS_DIRSIZ(fs, dp);
		if (lfs_dir_getreclen(fs, dp) == 0) {
			DIRHASH_UNLOCK(dh);
			brelse(bp, 0);
			return (-1);
		}
		i += lfs_dir_getreclen(fs, dp);
		dp = LFS_NEXTDIR(fs, dp);
	}
	if (i > dirblksiz) {
		DIRHASH_UNLOCK(dh);
		brelse(bp, 0);
		return (-1);
	}
	if (freebytes < slotneeded)
		panic("ulfsdirhash_findfree: free mismatch");
	DIRHASH_UNLOCK(dh);
	brelse(bp, 0);
	*slotsize = pos + i - slotstart;
	return (slotstart);
}

/*
 * Return the start of the unused space at the end of a directory, or
 * -1 if there are no trailing unused blocks.
 */
doff_t
ulfsdirhash_enduseful(struct inode *ip)
{
	struct dirhash *dh;
	int i;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return (-1);

	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return (-1);
	}

	if (dh->dh_blkfree[dh->dh_dirblks - 1] != dirblksiz / DIRALIGN) {
		DIRHASH_UNLOCK(dh);
		return (-1);
	}

	for (i = dh->dh_dirblks - 1; i >= 0; i--)
		if (dh->dh_blkfree[i] != dirblksiz / DIRALIGN)
			break;
	DIRHASH_UNLOCK(dh);
	return ((doff_t)(i + 1) * dirblksiz);
}

/*
 * Insert information into the hash about a new directory entry. dirp
 * points to a struct lfs_direct containing the entry, and offset specifies
 * the offset of this entry.
 */
void
ulfsdirhash_add(struct inode *ip, LFS_DIRHEADER *dirp, doff_t offset)
{
	struct lfs *fs = ip->i_lfs;
	struct dirhash *dh;
	int slot;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return;

	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	KASSERT(offset < dh->dh_dirblks * dirblksiz);
	/*
	 * Normal hash usage is < 66%. If the usage gets too high then
	 * remove the hash entirely and let it be rebuilt later.
	 */
	if (dh->dh_hused >= (dh->dh_hlen * 3) / 4) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	/* Find a free hash slot (empty or deleted), and add the entry. */
	slot = ulfsdirhash_hash(dh, lfs_dir_nameptr(fs, dirp),
				lfs_dir_getnamlen(fs, dirp));
	while (DH_ENTRY(dh, slot) >= 0)
		slot = WRAPINCR(slot, dh->dh_hlen);
	if (DH_ENTRY(dh, slot) == DIRHASH_EMPTY)
		dh->dh_hused++;
	DH_ENTRY(dh, slot) = offset;

	/* Update the per-block summary info. */
	ulfsdirhash_adjfree(dh, offset, -LFS_DIRSIZ(fs, dirp), dirblksiz);
	DIRHASH_UNLOCK(dh);
}

/*
 * Remove the specified directory entry from the hash. The entry to remove
 * is defined by the name in `dirp', which must exist at the specified
 * `offset' within the directory.
 */
void
ulfsdirhash_remove(struct inode *ip, LFS_DIRHEADER *dirp, doff_t offset)
{
	struct lfs *fs = ip->i_lfs;
	struct dirhash *dh;
	int slot;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return;

	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	KASSERT(offset < dh->dh_dirblks * dirblksiz);
	/* Find the entry */
	slot = ulfsdirhash_findslot(dh, lfs_dir_nameptr(fs, dirp),
				    lfs_dir_getnamlen(fs, dirp), offset);

	/* Remove the hash entry. */
	ulfsdirhash_delslot(dh, slot);

	/* Update the per-block summary info. */
	ulfsdirhash_adjfree(dh, offset, LFS_DIRSIZ(fs, dirp), dirblksiz);
	DIRHASH_UNLOCK(dh);
}

/*
 * Change the offset associated with a directory entry in the hash. Used
 * when compacting directory blocks.
 */
void
ulfsdirhash_move(struct inode *ip, LFS_DIRHEADER *dirp, doff_t oldoff,
    doff_t newoff)
{
	struct lfs *fs = ip->i_lfs;
	struct dirhash *dh;
	int slot;

	if ((dh = ip->i_dirhash) == NULL)
		return;
	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	KASSERT(oldoff < dh->dh_dirblks * ip->i_lfs->um_dirblksiz &&
	    newoff < dh->dh_dirblks * ip->i_lfs->um_dirblksiz);
	/* Find the entry, and update the offset. */
	slot = ulfsdirhash_findslot(dh, lfs_dir_nameptr(fs, dirp),
				    lfs_dir_getnamlen(fs, dirp), oldoff);
	DH_ENTRY(dh, slot) = newoff;
	DIRHASH_UNLOCK(dh);
}

/*
 * Inform dirhash that the directory has grown by one block that
 * begins at offset (i.e. the new length is offset + DIRBLKSIZ).
 */
void
ulfsdirhash_newblk(struct inode *ip, doff_t offset)
{
	struct dirhash *dh;
	int block;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return;
	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	KASSERT(offset == dh->dh_dirblks * dirblksiz);
	block = offset / dirblksiz;
	if (block >= dh->dh_nblk) {
		/* Out of space; must rebuild. */
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}
	dh->dh_dirblks = block + 1;

	/* Account for the new free block. */
	dh->dh_blkfree[block] = dirblksiz / DIRALIGN;
	if (dh->dh_firstfree[DH_NFSTATS] == -1)
		dh->dh_firstfree[DH_NFSTATS] = block;
	DIRHASH_UNLOCK(dh);
}

/*
 * Inform dirhash that the directory is being truncated.
 */
void
ulfsdirhash_dirtrunc(struct inode *ip, doff_t offset)
{
	struct dirhash *dh;
	int block, i;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if ((dh = ip->i_dirhash) == NULL)
		return;

	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	KASSERT(offset <= dh->dh_dirblks * dirblksiz);
	block = howmany(offset, dirblksiz);
	/*
	 * If the directory shrinks to less than 1/8 of dh_nblk blocks
	 * (about 20% of its original size due to the 50% extra added in
	 * ulfsdirhash_build) then free it, and let the caller rebuild
	 * if necessary.
	 */
	if (block < dh->dh_nblk / 8 && dh->dh_narrays > 1) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	/*
	 * Remove any `first free' information pertaining to the
	 * truncated blocks. All blocks we're removing should be
	 * completely unused.
	 */
	if (dh->dh_firstfree[DH_NFSTATS] >= block)
		dh->dh_firstfree[DH_NFSTATS] = -1;
	for (i = block; i < dh->dh_dirblks; i++)
		if (dh->dh_blkfree[i] != dirblksiz / DIRALIGN)
			panic("ulfsdirhash_dirtrunc: blocks in use");
	for (i = 0; i < DH_NFSTATS; i++)
		if (dh->dh_firstfree[i] >= block)
			panic("ulfsdirhash_dirtrunc: first free corrupt");
	dh->dh_dirblks = block;
	DIRHASH_UNLOCK(dh);
}

/*
 * Debugging function to check that the dirhash information about
 * a directory block matches its actual contents. Panics if a mismatch
 * is detected.
 *
 * On entry, `sbuf' should point to the start of an in-core
 * DIRBLKSIZ-sized directory block, and `offset' should contain the
 * offset from the start of the directory of that block.
 */
void
ulfsdirhash_checkblock(struct inode *ip, char *sbuf, doff_t offset)
{
	struct lfs *fs = ip->i_lfs;
	struct dirhash *dh;
	LFS_DIRHEADER *dp;
	int block, ffslot, i, nfree;
	int dirblksiz = ip->i_lfs->um_dirblksiz;

	if (!ulfs_dirhashcheck)
		return;
	if ((dh = ip->i_dirhash) == NULL)
		return;

	DIRHASH_LOCK(dh);
	if (dh->dh_hash == NULL) {
		DIRHASH_UNLOCK(dh);
		ulfsdirhash_free(ip);
		return;
	}

	block = offset / dirblksiz;
	if ((offset & (dirblksiz - 1)) != 0 || block >= dh->dh_dirblks)
		panic("ulfsdirhash_checkblock: bad offset");

	nfree = 0;
	for (i = 0; i < dirblksiz; i += lfs_dir_getreclen(fs, dp)) {
		dp = (LFS_DIRHEADER *)(sbuf + i);
		if (lfs_dir_getreclen(fs, dp) == 0 || i + lfs_dir_getreclen(fs, dp) > dirblksiz)
			panic("ulfsdirhash_checkblock: bad dir");

		if (lfs_dir_getino(fs, dp) == 0) {
#if 0
			/*
			 * XXX entries with d_ino == 0 should only occur
			 * at the start of a DIRBLKSIZ block. However the
			 * ulfs code is tolerant of such entries at other
			 * offsets, and fsck does not fix them.
			 */
			if (i != 0)
				panic("ulfsdirhash_checkblock: bad dir inode");
#endif
			nfree += lfs_dir_getreclen(fs, dp);
			continue;
		}

		/* Check that the entry	exists (will panic if it doesn't). */
		ulfsdirhash_findslot(dh, lfs_dir_nameptr(fs, dp),
				     lfs_dir_getnamlen(fs, dp),
				     offset + i);

		nfree += lfs_dir_getreclen(fs, dp) - LFS_DIRSIZ(fs, dp);
	}
	if (i != dirblksiz)
		panic("ulfsdirhash_checkblock: bad dir end");

	if (dh->dh_blkfree[block] * DIRALIGN != nfree)
		panic("ulfsdirhash_checkblock: bad free count");

	ffslot = BLKFREE2IDX(nfree / DIRALIGN);
	for (i = 0; i <= DH_NFSTATS; i++)
		if (dh->dh_firstfree[i] == block && i != ffslot)
			panic("ulfsdirhash_checkblock: bad first-free");
	if (dh->dh_firstfree[ffslot] == -1)
		panic("ulfsdirhash_checkblock: missing first-free entry");
	DIRHASH_UNLOCK(dh);
}

/*
 * Hash the specified filename into a dirhash slot.
 */
static int
ulfsdirhash_hash(struct dirhash *dh, const char *name, int namelen)
{
	u_int32_t hash;

	/*
	 * We hash the name and then some other bit of data that is
	 * invariant over the dirhash's lifetime. Otherwise names
	 * differing only in the last byte are placed close to one
	 * another in the table, which is bad for linear probing.
	 */
	hash = hash32_buf(name, namelen, HASH32_BUF_INIT);
	hash = hash32_buf(&dh, sizeof(dh), hash);
	return (hash % dh->dh_hlen);
}

/*
 * Adjust the number of free bytes in the block containing `offset'
 * by the value specified by `diff'.
 *
 * The caller must ensure we have exclusive access to `dh'; normally
 * that means that dh_lock should be held, but this is also called
 * from ulfsdirhash_build() where exclusive access can be assumed.
 */
static void
ulfsdirhash_adjfree(struct dirhash *dh, doff_t offset, int diff, int dirblksiz)
{
	int block, i, nfidx, ofidx;

	KASSERT(mutex_owned(&dh->dh_lock));

	/* Update the per-block summary info. */
	block = offset / dirblksiz;
	KASSERT(block < dh->dh_nblk && block < dh->dh_dirblks);
	ofidx = BLKFREE2IDX(dh->dh_blkfree[block]);
	dh->dh_blkfree[block] = (int)dh->dh_blkfree[block] + (diff / DIRALIGN);
	nfidx = BLKFREE2IDX(dh->dh_blkfree[block]);

	/* Update the `first free' list if necessary. */
	if (ofidx != nfidx) {
		/* If removing, scan forward for the next block. */
		if (dh->dh_firstfree[ofidx] == block) {
			for (i = block + 1; i < dh->dh_dirblks; i++)
				if (BLKFREE2IDX(dh->dh_blkfree[i]) == ofidx)
					break;
			dh->dh_firstfree[ofidx] = (i < dh->dh_dirblks) ? i : -1;
		}

		/* Make this the new `first free' if necessary */
		if (dh->dh_firstfree[nfidx] > block ||
		    dh->dh_firstfree[nfidx] == -1)
			dh->dh_firstfree[nfidx] = block;
	}
}

/*
 * Find the specified name which should have the specified offset.
 * Returns a slot number, and panics on failure.
 *
 * `dh' must be locked on entry and remains so on return.
 */
static int
ulfsdirhash_findslot(struct dirhash *dh, const char *name, int namelen,
    doff_t offset)
{
	int slot;

	KASSERT(mutex_owned(&dh->dh_lock));

	/* Find the entry. */
	KASSERT(dh->dh_hused < dh->dh_hlen);
	slot = ulfsdirhash_hash(dh, name, namelen);
	while (DH_ENTRY(dh, slot) != offset &&
	    DH_ENTRY(dh, slot) != DIRHASH_EMPTY)
		slot = WRAPINCR(slot, dh->dh_hlen);
	if (DH_ENTRY(dh, slot) != offset)
		panic("ulfsdirhash_findslot: '%.*s' not found", namelen, name);

	return (slot);
}

/*
 * Remove the entry corresponding to the specified slot from the hash array.
 *
 * `dh' must be locked on entry and remains so on return.
 */
static void
ulfsdirhash_delslot(struct dirhash *dh, int slot)
{
	int i;

	KASSERT(mutex_owned(&dh->dh_lock));

	/* Mark the entry as deleted. */
	DH_ENTRY(dh, slot) = DIRHASH_DEL;

	/* If this is the end of a chain of DIRHASH_DEL slots, remove them. */
	for (i = slot; DH_ENTRY(dh, i) == DIRHASH_DEL; )
		i = WRAPINCR(i, dh->dh_hlen);
	if (DH_ENTRY(dh, i) == DIRHASH_EMPTY) {
		i = WRAPDECR(i, dh->dh_hlen);
		while (DH_ENTRY(dh, i) == DIRHASH_DEL) {
			DH_ENTRY(dh, i) = DIRHASH_EMPTY;
			dh->dh_hused--;
			i = WRAPDECR(i, dh->dh_hlen);
		}
		KASSERT(dh->dh_hused >= 0);
	}
}

/*
 * Given a directory entry and its offset, find the offset of the
 * previous entry in the same DIRBLKSIZ-sized block. Returns an
 * offset, or -1 if there is no previous entry in the block or some
 * other problem occurred.
 */
static doff_t
ulfsdirhash_getprev(struct lfs *fs, LFS_DIRHEADER *dirp,
		doff_t offset, int dirblksiz)
{
	LFS_DIRHEADER *dp;
	char *blkbuf;
	doff_t blkoff, prevoff;
	int entrypos, i;
	unsigned reclen;

	blkoff = offset & ~(dirblksiz - 1);	/* offset of start of block */
	entrypos = offset & (dirblksiz - 1);	/* entry relative to block */
	blkbuf = (char *)dirp - entrypos;
	prevoff = blkoff;

	/* If `offset' is the start of a block, there is no previous entry. */
	if (entrypos == 0)
		return (-1);

	/* Scan from the start of the block until we get to the entry. */
	for (i = 0; i < entrypos; i += reclen) {
		dp = (LFS_DIRHEADER *)(blkbuf + i);
		reclen = lfs_dir_getreclen(fs, dp);
		if (reclen == 0 || i + reclen > entrypos)
			return (-1);	/* Corrupted directory. */
		prevoff = blkoff + i;
	}
	return (prevoff);
}

/*
 * Try to free up `wanted' bytes by stealing memory from existing
 * dirhashes. Returns zero with list locked if successful.
 */
static int
ulfsdirhash_recycle(int wanted)
{
	struct dirhash *dh;
	doff_t **hash;
	u_int8_t *blkfree;
	int i, mem, narrays;
	size_t hashsz, blkfreesz;

	DIRHASHLIST_LOCK();
	while (wanted + ulfs_dirhashmem > ulfs_dirhashmaxmem) {
		/* Find a dirhash, and lock it. */
		if ((dh = TAILQ_FIRST(&ulfsdirhash_list)) == NULL) {
			DIRHASHLIST_UNLOCK();
			return (-1);
		}
		DIRHASH_LOCK(dh);
		KASSERT(dh->dh_hash != NULL);

		/* Decrement the score; only recycle if it becomes zero. */
		if (--dh->dh_score > 0) {
			DIRHASH_UNLOCK(dh);
			DIRHASHLIST_UNLOCK();
			return (-1);
		}

		/* Remove it from the list and detach its memory. */
		TAILQ_REMOVE(&ulfsdirhash_list, dh, dh_list);
		dh->dh_onlist = 0;
		hash = dh->dh_hash;
		hashsz = dh->dh_hashsz;
		dh->dh_hash = NULL;
		blkfree = dh->dh_blkfree;
		blkfreesz = dh->dh_blkfreesz;
		dh->dh_blkfree = NULL;
		narrays = dh->dh_narrays;
		mem = narrays * sizeof(*dh->dh_hash) +
		    narrays * DH_NBLKOFF * sizeof(**dh->dh_hash) +
		    dh->dh_nblk * sizeof(*dh->dh_blkfree);

		/* Unlock everything, free the detached memory. */
		DIRHASH_UNLOCK(dh);
		DIRHASHLIST_UNLOCK();

		for (i = 0; i < narrays; i++)
			DIRHASH_BLKFREE(hash[i]);
		kmem_free(hash, hashsz);
		kmem_free(blkfree, blkfreesz);

		/* Account for the returned memory, and repeat if necessary. */
		DIRHASHLIST_LOCK();
		atomic_add_int(&ulfs_dirhashmem, -mem);
	}
	/* Success. */
	return (0);
}

static void
ulfsdirhash_sysctl_init(void)
{
	const struct sysctlnode *rnode, *cnode;

	sysctl_createv(&ulfsdirhash_sysctl_log, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "ulfs",
		       SYSCTL_DESCR("ulfs"),
		       NULL, 0, NULL, 0,
		       CTL_VFS, CTL_CREATE, CTL_EOL);

	sysctl_createv(&ulfsdirhash_sysctl_log, 0, &rnode, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "dirhash",
		       SYSCTL_DESCR("dirhash"),
		       NULL, 0, NULL, 0,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(&ulfsdirhash_sysctl_log, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "minblocks",
		       SYSCTL_DESCR("minimum hashed directory size in blocks"),
		       NULL, 0, &ulfs_dirhashminblks, 0,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(&ulfsdirhash_sysctl_log, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxmem",
		       SYSCTL_DESCR("maximum dirhash memory usage"),
		       NULL, 0, &ulfs_dirhashmaxmem, 0,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(&ulfsdirhash_sysctl_log, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READONLY,
		       CTLTYPE_INT, "memused",
		       SYSCTL_DESCR("current dirhash memory usage"),
		       NULL, 0, &ulfs_dirhashmem, 0,
		       CTL_CREATE, CTL_EOL);

	sysctl_createv(&ulfsdirhash_sysctl_log, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
		       CTLTYPE_INT, "docheck",
		       SYSCTL_DESCR("enable extra sanity checks"),
		       NULL, 0, &ulfs_dirhashcheck, 0,
		       CTL_CREATE, CTL_EOL);
}

void
ulfsdirhash_init(void)
{

	mutex_init(&ulfsdirhash_lock, MUTEX_DEFAULT, IPL_NONE);
	ulfsdirhashblk_cache = pool_cache_init(DH_NBLKOFF * sizeof(daddr_t), 0,
	    0, 0, "dirhashblk", NULL, IPL_NONE, NULL, NULL, NULL);
	ulfsdirhash_cache = pool_cache_init(sizeof(struct dirhash), 0,
	    0, 0, "dirhash", NULL, IPL_NONE, NULL, NULL, NULL);
	TAILQ_INIT(&ulfsdirhash_list);
	ulfsdirhash_sysctl_init();
}

void
ulfsdirhash_done(void)
{

	KASSERT(TAILQ_EMPTY(&ulfsdirhash_list));
	pool_cache_destroy(ulfsdirhashblk_cache);
	pool_cache_destroy(ulfsdirhash_cache);
	mutex_destroy(&ulfsdirhash_lock);
	sysctl_teardown(&ulfsdirhash_sysctl_log);
}
