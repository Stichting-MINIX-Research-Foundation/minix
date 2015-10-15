/*	$NetBSD: ulfs_lookup.c,v 1.34 2015/09/21 01:24:23 dholland Exp $	*/
/*  from NetBSD: ufs_lookup.c,v 1.122 2013/01/22 09:39:18 dholland Exp  */

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_lookup.c	8.9 (Berkeley) 8/11/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ulfs_lookup.c,v 1.34 2015/09/21 01:24:23 dholland Exp $");

#ifdef _KERNEL_OPT
#include "opt_lfs.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/kauth.h>
#include <sys/wapbl.h>
#include <sys/fstrans.h>
#include <sys/proc.h>
#include <sys/kmem.h>

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_extern.h>

#include <ufs/lfs/ulfs_inode.h>
#ifdef LFS_DIRHASH
#include <ufs/lfs/ulfs_dirhash.h>
#endif
#include <ufs/lfs/ulfsmount.h>
#include <ufs/lfs/ulfs_extern.h>
#include <ufs/lfs/ulfs_bswap.h>

#include <miscfs/genfs/genfs.h>

#ifdef DIAGNOSTIC
int	lfs_dirchk = 1;
#else
int	lfs_dirchk = 0;
#endif

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The cnp->cn_nameiop argument is LOOKUP, CREATE, RENAME, or DELETE depending
 * on whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and vput
 * instead of two vputs.
 *
 * Overall outline of ulfs_lookup:
 *
 *	check accessibility of directory
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 */
int
ulfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vdp = ap->a_dvp;	/* vnode for directory being searched */
	struct inode *dp = VTOI(vdp);	/* inode for directory being searched */
	struct buf *bp;			/* a buffer of directory entries */
	LFS_DIRHEADER *ep;		/* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	enum {
		NONE,		/* need to search a slot for our new entry */
		COMPACT,	/* a compaction can make a slot in the current
				   DIRBLKSIZ block */
		FOUND,		/* found a slot (or no need to search) */
	} slotstatus;
	doff_t slotoffset;		/* offset of area with free space.
					   a special value -1 for invalid */
	int slotsize;			/* size of area at slotoffset */
	int slotfreespace;		/* accumulated amount of space free in
					   the current DIRBLKSIZ block */
	int slotneeded;			/* size of the entry we're seeking */
	int numdirpasses;		/* strategy for directory search */
	doff_t endsearch;		/* offset to end directory search */
	doff_t prevoff;			/* previous value of ulr_offset */
	struct vnode *tdp;		/* returned by vcache_get */
	doff_t enduseful;		/* pointer past last used dir slot.
					   used for directory truncation. */
	u_long bmask;			/* block offset mask */
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	kauth_cred_t cred = cnp->cn_cred;
	int flags;
	int nameiop = cnp->cn_nameiop;
	struct lfs *fs = dp->i_lfs;
	int dirblksiz = fs->um_dirblksiz;
	ino_t foundino;
	struct ulfs_lookup_results *results;
	int iswhiteout;			/* temp result from cache_lookup() */

	flags = cnp->cn_flags;

	bp = NULL;
	slotoffset = -1;
	*vpp = NULL;
	endsearch = 0; /* silence compiler warning */

	/*
	 * Produce the auxiliary lookup results into i_crap. Increment
	 * its serial number so elsewhere we can tell if we're using
	 * stale results. This should not be done this way. XXX.
	 */
	results = &dp->i_crap;
	dp->i_crapcounter++;

	/*
	 * Check accessiblity of directory.
	 */
	if ((error = VOP_ACCESS(vdp, VEXEC, cred)) != 0)
		return (error);

	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (nameiop == DELETE || nameiop == RENAME))
		return (EROFS);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if (cache_lookup(vdp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags, &iswhiteout, vpp)) {
		if (iswhiteout) {
			cnp->cn_flags |= ISWHITEOUT;
		}
		return *vpp == NULLVP ? ENOENT : 0;
	}
	if (iswhiteout) {
		/*
		 * The namecache set iswhiteout without finding a
		 * cache entry. As of this writing (20121014), this
		 * can happen if there was a whiteout entry that has
		 * been invalidated by the lookup. It is not clear if
		 * it is correct to set ISWHITEOUT in this case or
		 * not; however, doing so retains the prior behavior,
		 * so we'll go with that until some clearer answer
		 * appears. XXX
		 */
		cnp->cn_flags |= ISWHITEOUT;
	}

	fstrans_start(vdp->v_mount, FSTRANS_SHARED);

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotstatus = FOUND;
	slotfreespace = slotsize = slotneeded = 0;
	if ((nameiop == CREATE || nameiop == RENAME) && (flags & ISLASTCN)) {
		slotstatus = NONE;
		slotneeded = LFS_DIRECTSIZ(fs, cnp->cn_namelen);
	}

	/*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	 */
	bmask = vdp->v_mount->mnt_stat.f_iosize - 1;

#ifdef LFS_DIRHASH
	/*
	 * Use dirhash for fast operations on large directories. The logic
	 * to determine whether to hash the directory is contained within
	 * ulfsdirhash_build(); a zero return means that it decided to hash
	 * this directory and it successfully built up the hash table.
	 */
	if (ulfsdirhash_build(dp) == 0) {
		/* Look for a free slot if needed. */
		enduseful = dp->i_size;
		if (slotstatus != FOUND) {
			slotoffset = ulfsdirhash_findfree(dp, slotneeded,
			    &slotsize);
			if (slotoffset >= 0) {
				slotstatus = COMPACT;
				enduseful = ulfsdirhash_enduseful(dp);
				if (enduseful < 0)
					enduseful = dp->i_size;
			}
		}
		/* Look up the component. */
		numdirpasses = 1;
		entryoffsetinblock = 0; /* silence compiler warning */
		switch (ulfsdirhash_lookup(dp, cnp->cn_nameptr, cnp->cn_namelen,
		    &results->ulr_offset, &bp, nameiop == DELETE ? &prevoff : NULL)) {
		case 0:
			ep = (LFS_DIRHEADER *)((char *)bp->b_data +
			    (results->ulr_offset & bmask));
			goto foundentry;
		case ENOENT:
			results->ulr_offset = roundup(dp->i_size, dirblksiz);
			goto notfound;
		default:
			/* Something failed; just do a linear search. */
			break;
		}
	}
#endif /* LFS_DIRHASH */

	if (nameiop != LOOKUP || results->ulr_diroff == 0 ||
	    results->ulr_diroff >= dp->i_size) {
		entryoffsetinblock = 0;
		results->ulr_offset = 0;
		numdirpasses = 1;
	} else {
		results->ulr_offset = results->ulr_diroff;
		if ((entryoffsetinblock = results->ulr_offset & bmask) &&
		    (error = ulfs_blkatoff(vdp, (off_t)results->ulr_offset,
		    NULL, &bp, false)))
			goto out;
		numdirpasses = 2;
		namecache_count_2passes();
	}
	prevoff = results->ulr_offset;
	endsearch = roundup(dp->i_size, dirblksiz);
	enduseful = 0;

searchloop:
	while (results->ulr_offset < endsearch) {
		if (curcpu()->ci_schedstate.spc_flags & SPCF_SHOULDYIELD)
			preempt();
		/*
		 * If necessary, get the next directory block.
		 */
		if ((results->ulr_offset & bmask) == 0) {
			if (bp != NULL)
				brelse(bp, 0);
			error = ulfs_blkatoff(vdp, (off_t)results->ulr_offset,
			    NULL, &bp, false);
			if (error)
				goto out;
			entryoffsetinblock = 0;
		}
		/*
		 * If still looking for a slot, and at a DIRBLKSIZ
		 * boundary, have to start looking for free space again.
		 */
		if (slotstatus == NONE &&
		    (entryoffsetinblock & (dirblksiz - 1)) == 0) {
			slotoffset = -1;
			slotfreespace = 0;
		}
		/*
		 * Get pointer to next entry.
		 * Full validation checks are slow, so we only check
		 * enough to insure forward progress through the
		 * directory. Complete checks can be run by patching
		 * "lfs_dirchk" to be true.
		 */
		KASSERT(bp != NULL);
		ep = (LFS_DIRHEADER *)((char *)bp->b_data + entryoffsetinblock);
		if (lfs_dir_getreclen(fs, ep) == 0 ||
		    (lfs_dirchk && ulfs_dirbadentry(vdp, ep, entryoffsetinblock))) {
			int i;

			ulfs_dirbad(dp, results->ulr_offset, "mangled entry");
			i = dirblksiz - (entryoffsetinblock & (dirblksiz - 1));
			results->ulr_offset += i;
			entryoffsetinblock += i;
			continue;
		}

		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		 */
		if (slotstatus != FOUND) {
			int size = lfs_dir_getreclen(fs, ep);

			if (lfs_dir_getino(fs, ep) != 0)
				size -= LFS_DIRSIZ(fs, ep);
			if (size > 0) {
				if (size >= slotneeded) {
					slotstatus = FOUND;
					slotoffset = results->ulr_offset;
					slotsize = lfs_dir_getreclen(fs, ep);
				} else if (slotstatus == NONE) {
					slotfreespace += size;
					if (slotoffset == -1)
						slotoffset = results->ulr_offset;
					if (slotfreespace >= slotneeded) {
						slotstatus = COMPACT;
						slotsize = results->ulr_offset +
						    lfs_dir_getreclen(fs, ep) -
						    slotoffset;
					}
				}
			}
		}

		/*
		 * Check for a name match.
		 */
		if (lfs_dir_getino(fs, ep)) {
			int namlen;

			namlen = lfs_dir_getnamlen(fs, ep);
			if (namlen == cnp->cn_namelen &&
			    !memcmp(cnp->cn_nameptr, lfs_dir_nameptr(fs, ep),
			    (unsigned)namlen)) {
#ifdef LFS_DIRHASH
foundentry:
#endif
				/*
				 * Save directory entry's inode number and
				 * reclen, and release directory buffer.
				 */
				if (!FSFMT(vdp) && lfs_dir_gettype(fs, ep) == LFS_DT_WHT) {
					slotstatus = FOUND;
					slotoffset = results->ulr_offset;
					slotsize = lfs_dir_getreclen(fs, ep);
					results->ulr_reclen = slotsize;
					/*
					 * This is used to set
					 * results->ulr_endoff,
					 * which may be used by ulfs_direnter()
					 * as a length to truncate the
					 * directory to.  Therefore, it must
					 * point past the end of the last
					 * non-empty directory entry.  We don't
					 * know where that is in this case, so
					 * we effectively disable shrinking by
					 * using the existing size of the
					 * directory.
					 *
					 * Note that we wouldn't expect to
					 * shrink the directory while rewriting
					 * an existing entry anyway.
					 */
					enduseful = endsearch;
					cnp->cn_flags |= ISWHITEOUT;
					numdirpasses--;
					goto notfound;
				}
				foundino = lfs_dir_getino(fs, ep);
				results->ulr_reclen = lfs_dir_getreclen(fs, ep);
				goto found;
			}
		}
		prevoff = results->ulr_offset;
		results->ulr_offset += lfs_dir_getreclen(fs, ep);
		entryoffsetinblock += lfs_dir_getreclen(fs, ep);
		if (lfs_dir_getino(fs, ep))
			enduseful = results->ulr_offset;
	}
notfound:
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		results->ulr_offset = 0;
		endsearch = results->ulr_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		brelse(bp, 0);
	/*
	 * If creating, and at end of pathname and current
	 * directory has not been removed, then can consider
	 * allowing file to be created.
	 */
	if ((nameiop == CREATE || nameiop == RENAME ||
	     (nameiop == DELETE &&
	      (cnp->cn_flags & DOWHITEOUT) &&
	      (cnp->cn_flags & ISWHITEOUT))) &&
	    (flags & ISLASTCN) && dp->i_nlink != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		error = VOP_ACCESS(vdp, VWRITE, cred);
		if (error)
			goto out;
		/*
		 * Return an indication of where the new directory
		 * entry should be put.  If we didn't find a slot,
		 * then set results->ulr_count to 0 indicating
		 * that the new slot belongs at the end of the
		 * directory. If we found a slot, then the new entry
		 * can be put in the range from results->ulr_offset to
		 * results->ulr_offset + results->ulr_count.
		 */
		if (slotstatus == NONE) {
			results->ulr_offset = roundup(dp->i_size, dirblksiz);
			results->ulr_count = 0;
			enduseful = results->ulr_offset;
		} else if (nameiop == DELETE) {
			results->ulr_offset = slotoffset;
			if ((results->ulr_offset & (dirblksiz - 1)) == 0)
				results->ulr_count = 0;
			else
				results->ulr_count =
				    results->ulr_offset - prevoff;
		} else {
			results->ulr_offset = slotoffset;
			results->ulr_count = slotsize;
			if (enduseful < slotoffset + slotsize)
				enduseful = slotoffset + slotsize;
		}
		results->ulr_endoff = roundup(enduseful, dirblksiz);
#if 0 /* commented out by dbj. none of the on disk fields changed */
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
#endif
		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		error = EJUSTRETURN;
		goto out;
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if (nameiop != CREATE) {
		cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);
	}
	error = ENOENT;
	goto out;

found:
	if (numdirpasses == 2)
		namecache_count_pass2();
	/*
	 * Check that directory length properly reflects presence
	 * of this entry.
	 */
	if (results->ulr_offset + LFS_DIRSIZ(fs, ep) > dp->i_size) {
		ulfs_dirbad(dp, results->ulr_offset, "i_size too small");
		dp->i_size =
		    results->ulr_offset + LFS_DIRSIZ(fs, ep);
		DIP_ASSIGN(dp, size, dp->i_size);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	brelse(bp, 0);

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if ((flags & ISLASTCN) && nameiop == LOOKUP)
		results->ulr_diroff = results->ulr_offset &~ (dirblksiz - 1);

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * Lock the inode, being careful with ".".
	 */
	if (nameiop == DELETE && (flags & ISLASTCN)) {
		/*
		 * Return pointer to current entry in results->ulr_offset,
		 * and distance past previous entry (if there
		 * is a previous entry in this block) in results->ulr_count.
		 * Save directory inode pointer in ndp->ni_dvp for dirremove().
		 */
		if ((results->ulr_offset & (dirblksiz - 1)) == 0)
			results->ulr_count = 0;
		else
			results->ulr_count = results->ulr_offset - prevoff;
		if (dp->i_number == foundino) {
			vref(vdp);
			tdp = vdp;
		} else {
			error = vcache_get(vdp->v_mount,
			    &foundino, sizeof(foundino), &tdp);
			if (error)
				goto out;
		}
		/*
		 * Write access to directory required to delete files.
		 */
		error = VOP_ACCESS(vdp, VWRITE, cred);
		if (error) {
			vrele(tdp);
			goto out;
		}
		/*
		 * If directory is "sticky", then user must own
		 * the directory, or the file in it, else she
		 * may not delete it (unless she's root). This
		 * implements append-only directories.
		 */
		if (dp->i_mode & ISVTX) {
			error = kauth_authorize_vnode(cred, KAUTH_VNODE_DELETE,
			    tdp, vdp, genfs_can_sticky(cred, dp->i_uid,
			    VTOI(tdp)->i_uid));
			if (error) {
				vrele(tdp);
				error = EPERM;
				goto out;
			}
		}
		*vpp = tdp;
		error = 0;
		goto out;
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (nameiop == RENAME && (flags & ISLASTCN)) {
		error = VOP_ACCESS(vdp, VWRITE, cred);
		if (error)
			goto out;
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->i_number == foundino) {
			error = EISDIR;
			goto out;
		}
		error = vcache_get(vdp->v_mount,
		    &foundino, sizeof(foundino), &tdp);
		if (error)
			goto out;
		*vpp = tdp;
		error = 0;
		goto out;
	}

	if (dp->i_number == foundino) {
		vref(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		error = vcache_get(vdp->v_mount,
		    &foundino, sizeof(foundino), &tdp);
		if (error)
			goto out;
		*vpp = tdp;
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_flags);
	error = 0;

out:
	fstrans_done(vdp->v_mount);
	return error;
}

void
ulfs_dirbad(struct inode *ip, doff_t offset, const char *how)
{
	struct mount *mp;

	mp = ITOV(ip)->v_mount;
	printf("%s: bad dir ino %llu at offset %d: %s\n",
	    mp->mnt_stat.f_mntonname, (unsigned long long)ip->i_number,
	    offset, how);
	if ((mp->mnt_flag & MNT_RDONLY) == 0)
		panic("bad dir");
}

/*
 * Do consistency checking on a directory entry:
 *	record length must be multiple of 4
 *	entry must fit in rest of its DIRBLKSIZ block
 *	record must be large enough to contain entry
 *	name is not longer than LFS_MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 */
int
ulfs_dirbadentry(struct vnode *dp, LFS_DIRHEADER *ep, int entryoffsetinblock)
{
	int i;
	int namlen;
	unsigned reclen;
	struct ulfsmount *ump = VFSTOULFS(dp->v_mount);
	struct lfs *fs = ump->um_lfs;
	int dirblksiz = fs->um_dirblksiz;
	const char *name;

	namlen = lfs_dir_getnamlen(fs, ep);
	reclen = lfs_dir_getreclen(fs, ep);
	if ((reclen & 0x3) != 0 ||
	    reclen > dirblksiz - (entryoffsetinblock & (dirblksiz - 1)) ||
	    reclen < LFS_DIRSIZ(fs, ep) || namlen > LFS_MAXNAMLEN) {
		/*return (1); */
		printf("First bad, reclen=%#x, DIRSIZ=%lu, namlen=%d, "
			"flags=%#x, entryoffsetinblock=%d, dirblksiz = %d\n",
			lfs_dir_getreclen(fs, ep),
			(u_long)LFS_DIRSIZ(fs, ep),
			namlen, dp->v_mount->mnt_flag, entryoffsetinblock,
			dirblksiz);
		goto bad;
	}
	if (lfs_dir_getino(fs, ep) == 0)
		return (0);
	name = lfs_dir_nameptr(fs, ep);
	for (i = 0; i < namlen; i++)
		if (name[i] == '\0') {
			/*return (1); */
			printf("Second bad\n");
			goto bad;
	}
	if (name[i])
		goto bad;
	return (0);
bad:
	return (1);
}

/*
 * Assign the contents of directory entry DIRP, on volume FS.
 *
 * NAME/NAMLEN is the name, which is not necessarily null terminated.
 * INUM is the inode number, and DTYPE is the type code (LFS_DT_*).
 *
 * Note that these values typically come from:
 *    cnp->cn_nameptr
 *    cnp->cn_namelen
 *    ip->i_number
 *    LFS_IFTODT(ip->i_mode)
 *
 * Does not set d_reclen.
 */
static void
ulfs_direntry_assign(struct lfs *fs, LFS_DIRHEADER *dirp,
		     const char *name, size_t namlen,
		     ino_t inum, unsigned dtype)
{
	lfs_dir_setino(fs, dirp, inum);
	lfs_dir_setnamlen(fs, dirp, namlen);
	lfs_dir_settype(fs, dirp, dtype);
	memcpy(lfs_dir_nameptr(fs, dirp), name, namlen);
	lfs_dir_nameptr(fs, dirp)[namlen] = '\0';
}

/*
 * Write a directory entry after a call to namei, using the parameters
 * that ulfs_lookup left in nameidata and in the ulfs_lookup_results.
 *
 * DVP is the directory to be updated. It must be locked.
 * ULR is the ulfs_lookup_results structure from the final lookup step.
 * TVP is not used. (XXX: why is it here? remove it)
 * CNP is the componentname from the final lookup step.
 * INUM is the inode number to insert into the new directory entry.
 * DTYPE is the type code (LFS_DT_*) to insert into the new directory entry.
 * NEWDIRBP is not used and (XXX) should be removed. The previous
 * comment here said it was used by the now-removed softupdates code.
 *
 * The link count of the target inode is *not* incremented; the
 * caller does that.
 *
 * If ulr->ulr_count is 0, ulfs_lookup did not find space to insert the
 * directory entry. ulr_offset, which is the place to put the entry,
 * should be on a block boundary (and should be at the end of the
 * directory AFAIK) and a fresh block is allocated to put the new
 * directory entry in.
 *
 * If ulr->ulr_count is not zero, ulfs_lookup found a slot to insert
 * the entry into. This slot ranges from ulr_offset to ulr_offset +
 * ulr_count. However, this slot may already be partially populated
 * requiring compaction. See notes below.
 *
 * Furthermore, if ulr_count is not zero and ulr_endoff is not the
 * same as i_size, the directory is truncated to size ulr_endoff.
 */
int
ulfs_direnter(struct vnode *dvp, const struct ulfs_lookup_results *ulr,
    struct vnode *tvp,
    struct componentname *cnp, ino_t inum, unsigned dtype,
    struct buf *newdirbp)
{
	kauth_cred_t cr;
	int newentrysize;
	struct inode *dp;
	struct buf *bp;
	u_int dsize;
	LFS_DIRHEADER *ep, *nep;
	int error, ret, lfs_blkoff, loc, spacefree;
	char *dirbuf;
	struct timespec ts;
	struct ulfsmount *ump = VFSTOULFS(dvp->v_mount);
	struct lfs *fs = ump->um_lfs;
	int dirblksiz = fs->um_dirblksiz;
	const char *name;
	unsigned namlen, reclen;
#ifdef LFS_DIRHASH
	int dohashadd;
#endif

	error = 0;
	name = cnp->cn_nameptr; /* note: not null-terminated */
	namlen = cnp->cn_namelen;
	cr = cnp->cn_cred;

	dp = VTOI(dvp);
	newentrysize = LFS_DIRECTSIZ(fs, namlen);

	if (ulr->ulr_count == 0) {
		/*
		 * If ulr_count is 0, then namei could find no
		 * space in the directory. Here, ulr_offset will
		 * be on a directory block boundary and we will write the
		 * new entry into a fresh block.
		 */
		if (ulr->ulr_offset & (dirblksiz - 1))
			panic("ulfs_direnter: newblk");
		if ((error = lfs_balloc(dvp, (off_t)ulr->ulr_offset, dirblksiz,
		    cr, B_CLRBUF | B_SYNC, &bp)) != 0) {
			return (error);
		}
		dp->i_size = ulr->ulr_offset + dirblksiz;
		DIP_ASSIGN(dp, size, dp->i_size);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
		uvm_vnp_setsize(dvp, dp->i_size);
		lfs_blkoff = ulr->ulr_offset & (ump->um_mountp->mnt_stat.f_iosize - 1);
		ep = (LFS_DIRHEADER *)((char *)bp->b_data + lfs_blkoff);
		ulfs_direntry_assign(fs, ep, name, namlen, inum, dtype);
		lfs_dir_setreclen(fs, ep, dirblksiz);
#ifdef LFS_DIRHASH
		if (dp->i_dirhash != NULL) {
			ulfsdirhash_newblk(dp, ulr->ulr_offset);
			ulfsdirhash_add(dp, ep, ulr->ulr_offset);
			ulfsdirhash_checkblock(dp, (char *)bp->b_data + lfs_blkoff,
			    ulr->ulr_offset);
		}
#endif
		error = VOP_BWRITE(bp->b_vp, bp);
		vfs_timestamp(&ts);
		ret = lfs_update(dvp, &ts, &ts, UPDATE_DIROP);
		if (error == 0)
			return (ret);
		return (error);
	}

	/*
	 * If ulr_count is non-zero, then namei found space for the new
	 * entry in the range ulr_offset to ulr_offset + ulr_count
	 * in the directory. To use this space, we may have to compact
	 * the entries located there, by copying them together towards the
	 * beginning of the block, leaving the free space in one usable
	 * chunk at the end.
	 */

	/*
	 * Increase size of directory if entry eats into new space.
	 * This should never push the size past a new multiple of
	 * DIRBLKSIZ.
	 *
	 * N.B. - THIS IS AN ARTIFACT OF 4.2 AND SHOULD NEVER HAPPEN.
	 */
	if (ulr->ulr_offset + ulr->ulr_count > dp->i_size) {
#ifdef DIAGNOSTIC
		printf("ulfs_direnter: reached 4.2-only block, "
		       "not supposed to happen\n");
#endif
		dp->i_size = ulr->ulr_offset + ulr->ulr_count;
		DIP_ASSIGN(dp, size, dp->i_size);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Get the block containing the space for the new directory entry.
	 */
	error = ulfs_blkatoff(dvp, (off_t)ulr->ulr_offset, &dirbuf, &bp, true);
	if (error) {
		return (error);
	}
	/*
	 * Find space for the new entry. In the simple case, the entry at
	 * offset base will have the space. If it does not, then namei
	 * arranged that compacting the region ulr_offset to
	 * ulr_offset + ulr_count would yield the space.
	 */
	ep = (LFS_DIRHEADER *)dirbuf;
	dsize = (lfs_dir_getino(fs, ep) != 0) ? LFS_DIRSIZ(fs, ep) : 0;
	spacefree = lfs_dir_getreclen(fs, ep) - dsize;
	for (loc = lfs_dir_getreclen(fs, ep); loc < ulr->ulr_count; ) {
		nep = (LFS_DIRHEADER *)(dirbuf + loc);

		/* Trim the existing slot (NB: dsize may be zero). */
		lfs_dir_setreclen(fs, ep, dsize);
		ep = LFS_NEXTDIR(fs, ep);

		reclen = lfs_dir_getreclen(fs, nep);
		loc += reclen;
		if (lfs_dir_getino(fs, nep) == 0) {
			/*
			 * A mid-block unused entry. Such entries are
			 * never created by the kernel, but fsck_ffs
			 * can create them (and it doesn't fix them).
			 *
			 * Add up the free space, and initialise the
			 * relocated entry since we don't memcpy it.
			 */
			spacefree += reclen;
			lfs_dir_setino(fs, ep, 0);
			dsize = 0;
			continue;
		}
		dsize = LFS_DIRSIZ(fs, nep);
		spacefree += reclen - dsize;
#ifdef LFS_DIRHASH
		if (dp->i_dirhash != NULL)
			ulfsdirhash_move(dp, nep,
			    ulr->ulr_offset + ((char *)nep - dirbuf),
			    ulr->ulr_offset + ((char *)ep - dirbuf));
#endif
		memcpy((void *)ep, (void *)nep, dsize);
	}
	/*
	 * Here, `ep' points to a directory entry containing `dsize' in-use
	 * bytes followed by `spacefree' unused bytes. If ep->d_ino == 0,
	 * then the entry is completely unused (dsize == 0). The value
	 * of ep->d_reclen is always indeterminate.
	 *
	 * Update the pointer fields in the previous entry (if any),
	 * copy in the new entry, and write out the block.
	 */
	if (lfs_dir_getino(fs, ep) == 0 ||
	    (lfs_dir_getino(fs, ep) == ULFS_WINO &&
	     memcmp(lfs_dir_nameptr(fs, ep), name, namlen) == 0)) {
		if (spacefree + dsize < newentrysize)
			panic("ulfs_direnter: compact1");
		reclen = spacefree + dsize;
#ifdef LFS_DIRHASH
		dohashadd = (lfs_dir_getino(fs, ep) == 0);
#endif
	} else {
		if (spacefree < newentrysize)
			panic("ulfs_direnter: compact2");
		reclen = spacefree;
		lfs_dir_setreclen(fs, ep, dsize);
		ep = LFS_NEXTDIR(fs, ep);
#ifdef LFS_DIRHASH
		dohashadd = 1;
#endif
	}

	ulfs_direntry_assign(fs, ep, name, namlen, inum, dtype);
	lfs_dir_setreclen(fs, ep, reclen);
#ifdef LFS_DIRHASH
	if (dp->i_dirhash != NULL && dohashadd)
		ulfsdirhash_add(dp, ep, ulr->ulr_offset + ((char *)ep - dirbuf));
	if (dp->i_dirhash != NULL)
		ulfsdirhash_checkblock(dp, dirbuf -
		    (ulr->ulr_offset & (dirblksiz - 1)),
		    ulr->ulr_offset & ~(dirblksiz - 1));
#endif
	error = VOP_BWRITE(bp->b_vp, bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	/*
	 * If all went well, and the directory can be shortened, proceed
	 * with the truncation. Note that we have to unlock the inode for
	 * the entry that we just entered, as the truncation may need to
	 * lock other inodes which can lead to deadlock if we also hold a
	 * lock on the newly entered node.
	 */
	if (error == 0 && ulr->ulr_endoff && ulr->ulr_endoff < dp->i_size) {
#ifdef LFS_DIRHASH
		if (dp->i_dirhash != NULL)
			ulfsdirhash_dirtrunc(dp, ulr->ulr_endoff);
#endif
		(void) lfs_truncate(dvp, (off_t)ulr->ulr_endoff, IO_SYNC, cr);
	}
	return (error);
}

/*
 * Remove a directory entry after a call to namei, using the
 * parameters that ulfs_lookup left in nameidata and in the
 * ulfs_lookup_results.
 *
 * DVP is the directory to be updated. It must be locked.
 * ULR is the ulfs_lookup_results structure from the final lookup step.
 * IP, if not null, is the inode being unlinked.
 * FLAGS may contain DOWHITEOUT.
 * ISRMDIR is not used and (XXX) should be removed.
 *
 * If FLAGS contains DOWHITEOUT the entry is replaced with a whiteout
 * instead of being cleared.
 *
 * ulr->ulr_offset contains the position of the directory entry
 * to be removed.
 *
 * ulr->ulr_reclen contains the size of the directory entry to be
 * removed.
 *
 * ulr->ulr_count contains the size of the *previous* directory
 * entry. This allows finding it, for free space management. If
 * ulr_count is 0, the target entry is at the beginning of the
 * directory. (Does this ever happen? The first entry should be ".",
 * which should only be removed at rmdir time. Does rmdir come here
 * to clear out the "." and ".." entries? Perhaps, but I doubt it.)
 *
 * The space is marked free by adding it to the record length (not
 * name length) of the preceding entry. If the first entry becomes
 * free, it is marked free by setting the inode number to 0.
 *
 * The link count of IP is decremented. Note that this is not the
 * inverse behavior of ulfs_direnter, which does not adjust link
 * counts. Sigh.
 */
int
ulfs_dirremove(struct vnode *dvp, const struct ulfs_lookup_results *ulr,
	      struct inode *ip, int flags, int isrmdir)
{
	struct inode *dp = VTOI(dvp);
	struct lfs *fs = dp->i_lfs;
	LFS_DIRHEADER *ep;
	struct buf *bp;
	int error;

	if (flags & DOWHITEOUT) {
		/*
		 * Whiteout entry: set d_ino to ULFS_WINO.
		 */
		error = ulfs_blkatoff(dvp, (off_t)ulr->ulr_offset, (void *)&ep,
				     &bp, true);
		if (error)
			return (error);
		lfs_dir_setino(fs, ep, ULFS_WINO);
		lfs_dir_settype(fs, ep, LFS_DT_WHT);
		goto out;
	}

	if ((error = ulfs_blkatoff(dvp,
	    (off_t)(ulr->ulr_offset - ulr->ulr_count), (void *)&ep, &bp, true)) != 0)
		return (error);

#ifdef LFS_DIRHASH
	/*
	 * Remove the dirhash entry. This is complicated by the fact
	 * that `ep' is the previous entry when ulr_count != 0.
	 */
	if (dp->i_dirhash != NULL)
		ulfsdirhash_remove(dp, (ulr->ulr_count == 0) ? ep :
		   LFS_NEXTDIR(fs, ep), ulr->ulr_offset);
#endif

	if (ulr->ulr_count == 0) {
		/*
		 * First entry in block: set d_ino to zero.
		 */
		lfs_dir_setino(fs, ep, 0);
	} else {
		/*
		 * Collapse new free space into previous entry.
		 */
		lfs_dir_setreclen(fs, ep,
			lfs_dir_getreclen(fs, ep) + ulr->ulr_reclen);
	}

#ifdef LFS_DIRHASH
	if (dp->i_dirhash != NULL) {
		int dirblksiz = ip->i_lfs->um_dirblksiz;
		ulfsdirhash_checkblock(dp, (char *)ep -
		    ((ulr->ulr_offset - ulr->ulr_count) & (dirblksiz - 1)),
		    ulr->ulr_offset & ~(dirblksiz - 1));
	}
#endif

out:
	if (ip) {
		ip->i_nlink--;
		DIP_ASSIGN(ip, nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
	}
	/*
	 * XXX did it ever occur to anyone that it might be a good
	 * idea to restore ip->i_nlink if this fails? Or something?
	 * Currently on error return from this function the state of
	 * ip->i_nlink depends on what happened, and callers
	 * definitely do not take this into account.
	 */
	error = VOP_BWRITE(bp->b_vp, bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	/*
	 * If the last named reference to a snapshot goes away,
	 * drop its snapshot reference so that it will be reclaimed
	 * when last open reference goes away.
	 */
	if (ip != 0 && (ip->i_flags & SF_SNAPSHOT) != 0 &&
	    ip->i_nlink == 0)
		ulfs_snapgone(ip);
	return (error);
}

/*
 * Rewrite an existing directory entry to point at the inode supplied.
 *
 * DP is the directory to update.
 * OFFSET is the position of the entry in question. It may come
 * from ulr_offset of a ulfs_lookup_results.
 * OIP is the old inode the directory previously pointed to.
 * NEWINUM is the number of the new inode.
 * NEWTYPE is the new value for the type field of the directory entry.
 * (This is ignored if the fs doesn't support that.)
 * ISRMDIR is not used and (XXX) should be removed.
 * IFLAGS are added to DP's inode flags.
 *
 * The link count of OIP is decremented. Note that the link count of
 * the new inode is *not* incremented. Yay for symmetry.
 */
int
ulfs_dirrewrite(struct inode *dp, off_t offset,
    struct inode *oip, ino_t newinum, int newtype,
    int isrmdir, int iflags)
{
	struct lfs *fs = dp->i_lfs;
	struct buf *bp;
	LFS_DIRHEADER *ep;
	struct vnode *vdp = ITOV(dp);
	int error;

	error = ulfs_blkatoff(vdp, offset, (void *)&ep, &bp, true);
	if (error)
		return (error);
	lfs_dir_setino(fs, ep, newinum);
	lfs_dir_settype(fs, ep, newtype);
	oip->i_nlink--;
	DIP_ASSIGN(oip, nlink, oip->i_nlink);
	oip->i_flag |= IN_CHANGE;
	error = VOP_BWRITE(bp->b_vp, bp);
	dp->i_flag |= iflags;
	/*
	 * If the last named reference to a snapshot goes away,
	 * drop its snapshot reference so that it will be reclaimed
	 * when last open reference goes away.
	 */
	if ((oip->i_flags & SF_SNAPSHOT) != 0 && oip->i_nlink == 0)
		ulfs_snapgone(oip);
	return (error);
}

/*
 * Check if a directory is empty or not.
 * Inode supplied must be locked.
 *
 * Using a struct lfs_dirtemplate here is not precisely
 * what we want, but better than using a struct lfs_direct.
 *
 * NB: does not handle corrupted directories.
 */
int
ulfs_dirempty(struct inode *ip, ino_t parentino, kauth_cred_t cred)
{
	struct lfs *fs = ip->i_lfs;
	doff_t off;
	union lfs_dirtemplate dbuf;
	LFS_DIRHEADER *dp = (LFS_DIRHEADER *)&dbuf;
	int error, namlen;
	const char *name;
	size_t count;
/* XXX this should probably use LFS_DIRECTSIZ(fs, 2) */
#define	MINDIRSIZ (sizeof (struct lfs_dirtemplate64) / 2)

	for (off = 0; off < ip->i_size; off += lfs_dir_getreclen(fs, dp)) {
		error = ulfs_bufio(UIO_READ, ITOV(ip), (void *)dp, MINDIRSIZ,
		    off, IO_NODELOCKED, cred, &count, NULL);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (error || count != 0)
			return (0);
		/* avoid infinite loops */
		if (lfs_dir_getreclen(fs, dp) == 0)
			return (0);
		/* skip empty entries */
		if (lfs_dir_getino(fs, dp) == 0 ||
		    lfs_dir_getino(fs, dp) == ULFS_WINO)
			continue;
		/* accept only "." and ".." */
		namlen = lfs_dir_getnamlen(fs, dp);
		name = lfs_dir_nameptr(fs, dp);
		if (namlen > 2)
			return (0);
		if (name[0] != '.')
			return (0);
		/*
		 * At this point namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (namlen == 1 && lfs_dir_getino(fs, dp) == ip->i_number)
			continue;
		if (name[1] == '.' && lfs_dir_getino(fs, dp) == parentino)
			continue;
		return (0);
	}
	return (1);
}

#define	ULFS_DIRRABLKS 0
int ulfs_dirrablks = ULFS_DIRRABLKS;

/*
 * ulfs_blkatoff: Return buffer with the contents of block "offset" from
 * the beginning of directory "vp".  If "res" is non-NULL, fill it in with
 * a pointer to the remaining space in the directory.  If the caller intends
 * to modify the buffer returned, "modify" must be true.
 */

int
ulfs_blkatoff(struct vnode *vp, off_t offset, char **res, struct buf **bpp,
    bool modify)
{
	struct inode *ip __diagused;
	struct buf *bp;
	daddr_t lbn;
	const int dirrablks = ulfs_dirrablks;
	daddr_t *blks;
	int *blksizes;
	int run, error;
	struct mount *mp = vp->v_mount;
	const int bshift = mp->mnt_fs_bshift;
	const int bsize = 1 << bshift;
	off_t eof;

	blks = kmem_alloc((1 + dirrablks) * sizeof(daddr_t), KM_SLEEP);
	blksizes = kmem_alloc((1 + dirrablks) * sizeof(int), KM_SLEEP);
	ip = VTOI(vp);
	KASSERT(vp->v_size == ip->i_size);
	GOP_SIZE(vp, vp->v_size, &eof, 0);
	lbn = offset >> bshift;

	for (run = 0; run <= dirrablks;) {
		const off_t curoff = lbn << bshift;
		const int size = MIN(eof - curoff, bsize);

		if (size == 0) {
			break;
		}
		KASSERT(curoff < eof);
		blks[run] = lbn;
		blksizes[run] = size;
		lbn++;
		run++;
		if (size != bsize) {
			break;
		}
	}
	KASSERT(run >= 1);
	error = breadn(vp, blks[0], blksizes[0], &blks[1], &blksizes[1],
	    run - 1, (modify ? B_MODIFY : 0), &bp);
	if (error != 0) {
		*bpp = NULL;
		goto out;
	}
	if (res) {
		*res = (char *)bp->b_data + (offset & (bsize - 1));
	}
	*bpp = bp;

 out:
	kmem_free(blks, (1 + dirrablks) * sizeof(daddr_t));
	kmem_free(blksizes, (1 + dirrablks) * sizeof(int));
	return error;
}
