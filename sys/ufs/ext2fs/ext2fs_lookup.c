/*	$NetBSD: ext2fs_lookup.c,v 1.73 2013/01/22 09:39:15 dholland Exp $	*/

/*
 * Modified for NetBSD 1.2E
 * May 1997, Manuel Bouyer
 * Laboratoire d'informatique de Paris VI
 */
/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
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
 *	@(#)ufs_lookup.c	8.6 (Berkeley) 4/1/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ext2fs_lookup.c,v 1.73 2013/01/22 09:39:15 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/kauth.h>
#include <sys/proc.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs.h>

#include <miscfs/genfs/genfs.h>

extern	int dirchk;

static void	ext2fs_dirconv2ffs(struct ext2fs_direct *e2dir,
					  struct dirent *ffsdir);
static int	ext2fs_dirbadentry(struct vnode *dp,
					  struct ext2fs_direct *de,
					  int entryoffsetinblock);

/*
 * the problem that is tackled below is the fact that FFS
 * includes the terminating zero on disk while EXT2FS doesn't
 * this implies that we need to introduce some padding.
 * For instance, a filename "sbin" has normally a reclen 12
 * in EXT2, but 16 in FFS.
 * This reminds me of that Pepsi commercial: 'Kid saved a lousy nine cents...'
 * If it wasn't for that, the complete ufs code for directories would
 * have worked w/o changes (except for the difference in DIRBLKSIZ)
 */
static void
ext2fs_dirconv2ffs(struct ext2fs_direct *e2dir, struct dirent *ffsdir)
{
	memset(ffsdir, 0, sizeof(struct dirent));
	ffsdir->d_fileno = fs2h32(e2dir->e2d_ino);
	ffsdir->d_namlen = e2dir->e2d_namlen;

	ffsdir->d_type = DT_UNKNOWN;		/* don't know more here */
#ifdef DIAGNOSTIC
#if MAXNAMLEN < E2FS_MAXNAMLEN
	/*
	 * we should handle this more gracefully !
	 */
	if (e2dir->e2d_namlen > MAXNAMLEN)
		panic("ext2fs: e2dir->e2d_namlen");
#endif
#endif
	strncpy(ffsdir->d_name, e2dir->e2d_name, ffsdir->d_namlen);

	/* Godmar thinks: since e2dir->e2d_reclen can be big and means
	   nothing anyway, we compute our own reclen according to what
	   we think is right
	 */
	ffsdir->d_reclen = _DIRENT_SIZE(ffsdir);
}

/*
 * Vnode op for reading directories.
 *
 * Convert the on-disk entries to <sys/dirent.h> entries.
 * the problem is that the conversion will blow up some entries by four bytes,
 * so it can't be done in place. This is too bad. Right now the conversion is
 * done entry by entry, the converted entry is sent via uiomove.
 *
 * XXX allocate a buffer, convert as many entries as possible, then send
 * the whole buffer to uiomove
 */
int
ext2fs_readdir(void *v)
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		kauth_cred_t a_cred;
		int **a_eofflag;
		off_t **a_cookies;
		int ncookies;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	int error;
	size_t e2fs_count, readcnt;
	struct vnode *vp = ap->a_vp;
	struct m_ext2fs *fs = VTOI(vp)->i_e2fs;

	struct ext2fs_direct *dp;
	struct dirent *dstd;
	struct uio auio;
	struct iovec aiov;
	void *dirbuf;
	off_t off = uio->uio_offset;
	off_t *cookies = NULL;
	int nc = 0, ncookies = 0;
	int e2d_reclen;

	if (vp->v_type != VDIR)
		return (ENOTDIR);

	e2fs_count = uio->uio_resid;
	/* Make sure we don't return partial entries. */
	e2fs_count -= (uio->uio_offset + e2fs_count) & (fs->e2fs_bsize -1);
	if (e2fs_count <= 0)
		return (EINVAL);

	auio = *uio;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_len = e2fs_count;
	auio.uio_resid = e2fs_count;
	UIO_SETUP_SYSSPACE(&auio);
	dirbuf = kmem_alloc(e2fs_count, KM_SLEEP);
	dstd = kmem_zalloc(sizeof(struct dirent), KM_SLEEP);
	if (ap->a_ncookies) {
		nc = e2fs_count / _DIRENT_MINSIZE((struct dirent *)0);
		ncookies = nc;
		cookies = malloc(sizeof (off_t) * ncookies, M_TEMP, M_WAITOK);
		*ap->a_cookies = cookies;
	}
	aiov.iov_base = dirbuf;

	error = VOP_READ(ap->a_vp, &auio, 0, ap->a_cred);
	if (error == 0) {
		readcnt = e2fs_count - auio.uio_resid;
		for (dp = (struct ext2fs_direct *)dirbuf;
			(char *)dp < (char *)dirbuf + readcnt; ) {
			e2d_reclen = fs2h16(dp->e2d_reclen);
			if (e2d_reclen == 0) {
				error = EIO;
				break;
			}
			ext2fs_dirconv2ffs(dp, dstd);
			if(dstd->d_reclen > uio->uio_resid) {
				break;
			}
			error = uiomove(dstd, dstd->d_reclen, uio);
			if (error != 0) {
				break;
			}
			off = off + e2d_reclen;
			if (cookies != NULL) {
				*cookies++ = off;
				if (--ncookies <= 0){
					break;  /* out of cookies */
				}
			}
			/* advance dp */
			dp = (struct ext2fs_direct *) ((char *)dp + e2d_reclen);
		}
		/* we need to correct uio_offset */
		uio->uio_offset = off;
	}
	kmem_free(dirbuf, e2fs_count);
	kmem_free(dstd, sizeof(*dstd));
	*ap->a_eofflag = ext2fs_size(VTOI(ap->a_vp)) <= uio->uio_offset;
	if (ap->a_ncookies) {
		if (error) {
			free(*ap->a_cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		} else
			*ap->a_ncookies = nc - ncookies;
	}
	return (error);
}

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
 * Overall outline of ext2fs_lookup:
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
ext2fs_lookup(void *v)
{
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vdp = ap->a_dvp;	/* vnode for directory being searched */
	struct inode *dp = VTOI(vdp);	/* inode for directory being searched */
	struct buf *bp;			/* a buffer of directory entries */
	struct ext2fs_direct *ep;	/* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	enum {NONE, COMPACT, FOUND} slotstatus;
	doff_t slotoffset;		/* offset of area with free space */
	int slotsize;			/* size of area at slotoffset */
	int slotfreespace;		/* amount of space free in slot */
	int slotneeded;			/* size of the entry we're seeking */
	int numdirpasses;		/* strategy for directory search */
	doff_t endsearch;		/* offset to end directory search */
	doff_t prevoff;			/* prev entry dp->i_offset */
	struct vnode *pdp;		/* saved dp during symlink work */
	struct vnode *tdp;		/* returned by VFS_VGET */
	doff_t enduseful;		/* pointer past last used dir slot */
	u_long bmask;			/* block offset mask */
	int namlen, error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	kauth_cred_t cred = cnp->cn_cred;
	int flags;
	int nameiop = cnp->cn_nameiop;
	struct ufsmount *ump = dp->i_ump;
	int dirblksiz = ump->um_dirblksiz;
	ino_t foundino;
	struct ufs_lookup_results *results;

	flags = cnp->cn_flags;

	bp = NULL;
	slotoffset = -1;
	*vpp = NULL;

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
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if (cache_lookup(vdp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags, NULL, vpp)) {
		return *vpp == NULLVP ? ENOENT : 0;
	}

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	slotstatus = FOUND;
	slotfreespace = slotsize = slotneeded = 0;
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN)) {
		slotstatus = NONE;
		slotneeded = EXT2FS_DIRSIZ(cnp->cn_namelen);
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
	if (nameiop != LOOKUP || results->ulr_diroff == 0 ||
	    results->ulr_diroff >= ext2fs_size(dp)) {
		entryoffsetinblock = 0;
		results->ulr_offset = 0;
		numdirpasses = 1;
	} else {
		results->ulr_offset = results->ulr_diroff;
		if ((entryoffsetinblock = results->ulr_offset & bmask) &&
		    (error = ext2fs_blkatoff(vdp, (off_t)results->ulr_offset, NULL, &bp)))
			return (error);
		numdirpasses = 2;
		nchstats.ncs_2passes++;
	}
	prevoff = results->ulr_offset;
	endsearch = roundup(ext2fs_size(dp), dirblksiz);
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
			error = ext2fs_blkatoff(vdp, (off_t)results->ulr_offset, NULL,
			    &bp);
			if (error != 0)
				return (error);
			entryoffsetinblock = 0;
		}
		/*
		 * If still looking for a slot, and at a dirblksize
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
		 * "dirchk" to be true.
		 */
		KASSERT(bp != NULL);
		ep = (struct ext2fs_direct *)
			((char *)bp->b_data + entryoffsetinblock);
		if (ep->e2d_reclen == 0 ||
		    (dirchk &&
		     ext2fs_dirbadentry(vdp, ep, entryoffsetinblock))) {
			int i;

			ufs_dirbad(dp, results->ulr_offset, "mangled entry");
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
			int size = fs2h16(ep->e2d_reclen);

			if (ep->e2d_ino != 0)
				size -= EXT2FS_DIRSIZ(ep->e2d_namlen);
			if (size > 0) {
				if (size >= slotneeded) {
					slotstatus = FOUND;
					slotoffset = results->ulr_offset;
					slotsize = fs2h16(ep->e2d_reclen);
				} else if (slotstatus == NONE) {
					slotfreespace += size;
					if (slotoffset == -1)
						slotoffset = results->ulr_offset;
					if (slotfreespace >= slotneeded) {
						slotstatus = COMPACT;
						slotsize = results->ulr_offset +
						    fs2h16(ep->e2d_reclen) -
						    slotoffset;
					}
				}
			}
		}

		/*
		 * Check for a name match.
		 */
		if (ep->e2d_ino) {
			namlen = ep->e2d_namlen;
			if (namlen == cnp->cn_namelen &&
			    !memcmp(cnp->cn_nameptr, ep->e2d_name,
			    (unsigned)namlen)) {
				/*
				 * Save directory entry's inode number and
				 * reclen in ndp->ni_ufs area, and release
				 * directory buffer.
				 */
				foundino = fs2h32(ep->e2d_ino);
				results->ulr_reclen = fs2h16(ep->e2d_reclen);
				goto found;
			}
		}
		prevoff = results->ulr_offset;
		results->ulr_offset += fs2h16(ep->e2d_reclen);
		entryoffsetinblock += fs2h16(ep->e2d_reclen);
		if (ep->e2d_ino)
			enduseful = results->ulr_offset;
	}
/* notfound: */
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
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN) && dp->i_e2fs_nlink != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		error = VOP_ACCESS(vdp, VWRITE, cred);
		if (error)
			return (error);
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
			results->ulr_offset = roundup(ext2fs_size(dp), dirblksiz);
			results->ulr_count = 0;
			enduseful = results->ulr_offset;
		} else {
			results->ulr_offset = slotoffset;
			results->ulr_count = slotsize;
			if (enduseful < slotoffset + slotsize)
				enduseful = slotoffset + slotsize;
		}
		results->ulr_endoff = roundup(enduseful, dirblksiz);
#if 0
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
		return (EJUSTRETURN);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if (nameiop != CREATE) {
		cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);
	}
	return ENOENT;

found:
	if (numdirpasses == 2)
		nchstats.ncs_pass2++;
	/*
	 * Check that directory length properly reflects presence
	 * of this entry.
	 */
	if (results->ulr_offset + EXT2FS_DIRSIZ(ep->e2d_namlen) > ext2fs_size(dp)) {
		ufs_dirbad(dp, results->ulr_offset, "i_size too small");
		error = ext2fs_setsize(dp,
				results->ulr_offset + EXT2FS_DIRSIZ(ep->e2d_namlen));
		if (error) {
			brelse(bp, 0);
			return (error);
		}
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
		uvm_vnp_setsize(vdp, ext2fs_size(dp));
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
			if (flags & ISDOTDOT)
				VOP_UNLOCK(vdp); /* race to get the inode */
			error = VFS_VGET(vdp->v_mount, foundino, &tdp);
			if (flags & ISDOTDOT)
				vn_lock(vdp, LK_EXCLUSIVE | LK_RETRY);
			if (error)
				return (error);
		}
		/*
		 * Write access to directory required to delete files.
		 */
		if ((error = VOP_ACCESS(vdp, VWRITE, cred)) != 0) {
			if (dp->i_number == foundino)
				vrele(tdp);
			else
				vput(tdp);
			return (error);
		}
		/*
		 * If directory is "sticky", then user must own
		 * the directory, or the file in it, else she
		 * may not delete it (unless she's root). This
		 * implements append-only directories.
		 */
		if (dp->i_e2fs_mode & ISVTX) {
			error = kauth_authorize_vnode(cred, KAUTH_VNODE_DELETE,
			    tdp, vdp, genfs_can_sticky(cred, dp->i_uid,
			    VTOI(tdp)->i_uid));
			if (error) {
				if (dp->i_number == foundino)
					vrele(tdp);
				else
					vput(tdp);
				return (EPERM);
			}
		}
		*vpp = tdp;
		return (0);
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
			return (error);
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->i_number == foundino)
			return (EISDIR);
		if (flags & ISDOTDOT)
			VOP_UNLOCK(vdp); /* race to get the inode */
		error = VFS_VGET(vdp->v_mount, foundino, &tdp);
		if (flags & ISDOTDOT)
			vn_lock(vdp, LK_EXCLUSIVE | LK_RETRY);
		if (error)
			return (error);
		*vpp = tdp;
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `vput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the VFS_VGET for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = vdp;
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(pdp);	/* race to get the inode */
		error = VFS_VGET(vdp->v_mount, foundino, &tdp);
		vn_lock(pdp, LK_EXCLUSIVE | LK_RETRY);
		if (error) {
			return (error);
		}
		*vpp = tdp;
	} else if (dp->i_number == foundino) {
		vref(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		error = VFS_VGET(vdp->v_mount, foundino, &tdp);
		if (error)
			return (error);
		*vpp = tdp;
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_flags);
	return 0;
}

/*
 * Do consistency checking on a directory entry:
 *	record length must be multiple of 4
 *	entry must fit in rest of its dirblksize block
 *	record must be large enough to contain entry
 *	name is not longer than EXT2FS_MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 */
/*
 *	changed so that it confirms to ext2fs_check_dir_entry
 */
static int
ext2fs_dirbadentry(struct vnode *dp, struct ext2fs_direct *de,
		int entryoffsetinblock)
{
	struct ufsmount *ump = VFSTOUFS(dp->v_mount);
	int dirblksiz = ump->um_dirblksiz;

		const char *error_msg = NULL;
		int reclen = fs2h16(de->e2d_reclen);
		int namlen = de->e2d_namlen;

		if (reclen < EXT2FS_DIRSIZ(1)) /* e2d_namlen = 1 */
			error_msg = "rec_len is smaller than minimal";
		else if (reclen % 4 != 0)
			error_msg = "rec_len % 4 != 0";
		else if (namlen > EXT2FS_MAXNAMLEN)
			error_msg = "namlen > EXT2FS_MAXNAMLEN";
		else if (reclen < EXT2FS_DIRSIZ(namlen))
			error_msg = "reclen is too small for name_len";
		else if (entryoffsetinblock + reclen > dirblksiz)
			error_msg = "directory entry across blocks";
		else if (fs2h32(de->e2d_ino) >
		    VTOI(dp)->i_e2fs->e2fs.e2fs_icount)
			error_msg = "inode out of bounds";

		if (error_msg != NULL) {
			printf( "bad directory entry: %s\n"
			    "offset=%d, inode=%lu, rec_len=%d, name_len=%d \n",
			    error_msg, entryoffsetinblock,
			    (unsigned long) fs2h32(de->e2d_ino),
			    reclen, namlen);
			panic("ext2fs_dirbadentry");
		}
		return error_msg == NULL ? 0 : 1;
}

/*
 * Write a directory entry after a call to namei, using the parameters
 * that it left in nameidata.  The argument ip is the inode which the new
 * directory entry will refer to.  Dvp is a pointer to the directory to
 * be written, which was left locked by namei. Remaining parameters
 * (ulr_offset, ulr_count) indicate how the space for the new
 * entry is to be obtained.
 */
int
ext2fs_direnter(struct inode *ip, struct vnode *dvp,
		const struct ufs_lookup_results *ulr,
		struct componentname *cnp)
{
	struct ext2fs_direct *ep, *nep;
	struct inode *dp;
	struct buf *bp;
	struct ext2fs_direct newdir;
	struct iovec aiov;
	struct uio auio;
	u_int dsize;
	int error, loc, newentrysize, spacefree;
	char *dirbuf;
	struct ufsmount *ump = VFSTOUFS(dvp->v_mount);
	int dirblksiz = ump->um_dirblksiz;

	dp = VTOI(dvp);

	newdir.e2d_ino = h2fs32(ip->i_number);
	newdir.e2d_namlen = cnp->cn_namelen;
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    (ip->i_e2fs->e2fs.e2fs_features_incompat & EXT2F_INCOMPAT_FTYPE)) {
		newdir.e2d_type = inot2ext2dt(IFTODT(ip->i_e2fs_mode));
	} else {
		newdir.e2d_type = 0;
	}
	memcpy(newdir.e2d_name, cnp->cn_nameptr, (unsigned)cnp->cn_namelen + 1);
	newentrysize = EXT2FS_DIRSIZ(cnp->cn_namelen);
	if (ulr->ulr_count == 0) {
		/*
		 * If ulr_count is 0, then namei could find no
		 * space in the directory. Here, ulr_offset will
		 * be on a directory block boundary and we will write the
		 * new entry into a fresh block.
		 */
		if (ulr->ulr_offset & (dirblksiz - 1))
			panic("ext2fs_direnter: newblk");
		auio.uio_offset = ulr->ulr_offset;
		newdir.e2d_reclen = h2fs16(dirblksiz);
		auio.uio_resid = newentrysize;
		aiov.iov_len = newentrysize;
		aiov.iov_base = (void *)&newdir;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_rw = UIO_WRITE;
		UIO_SETUP_SYSSPACE(&auio);
		error = VOP_WRITE(dvp, &auio, IO_SYNC, cnp->cn_cred);
		if (dirblksiz > dvp->v_mount->mnt_stat.f_bsize)
			/* XXX should grow with balloc() */
			panic("ext2fs_direnter: frag size");
		else if (!error) {
			error = ext2fs_setsize(dp,
				roundup(ext2fs_size(dp), dirblksiz));
			if (error)
				return (error);
			dp->i_flag |= IN_CHANGE;
			uvm_vnp_setsize(dvp, ext2fs_size(dp));
		}
		return (error);
	}

	/*
	 * If ulr_count is non-zero, then namei found space
	 * for the new entry in the range ulr_offset to
	 * ulr_offset + ulr_count in the directory.
	 * To use this space, we may have to compact the entries located
	 * there, by copying them together towards the beginning of the
	 * block, leaving the free space in one usable chunk at the end.
	 */

	/*
	 * Get the block containing the space for the new directory entry.
	 */
	if ((error = ext2fs_blkatoff(dvp, (off_t)ulr->ulr_offset, &dirbuf, &bp)) != 0)
		return (error);
	/*
	 * Find space for the new entry. In the simple case, the entry at
	 * offset base will have the space. If it does not, then namei
	 * arranged that compacting the region ulr_offset to
	 * ulr_offset + ulr_count would yield the
	 * space.
	 */
	ep = (struct ext2fs_direct *)dirbuf;
	dsize = EXT2FS_DIRSIZ(ep->e2d_namlen);
	spacefree = fs2h16(ep->e2d_reclen) - dsize;
	for (loc = fs2h16(ep->e2d_reclen); loc < ulr->ulr_count; ) {
		nep = (struct ext2fs_direct *)(dirbuf + loc);
		if (ep->e2d_ino) {
			/* trim the existing slot */
			ep->e2d_reclen = h2fs16(dsize);
			ep = (struct ext2fs_direct *)((char *)ep + dsize);
		} else {
			/* overwrite; nothing there; header is ours */
			spacefree += dsize;
		}
		dsize = EXT2FS_DIRSIZ(nep->e2d_namlen);
		spacefree += fs2h16(nep->e2d_reclen) - dsize;
		loc += fs2h16(nep->e2d_reclen);
		memcpy((void *)ep, (void *)nep, dsize);
	}
	/*
	 * Update the pointer fields in the previous entry (if any),
	 * copy in the new entry, and write out the block.
	 */
	if (ep->e2d_ino == 0) {
#ifdef DIAGNOSTIC
		if (spacefree + dsize < newentrysize)
			panic("ext2fs_direnter: compact1");
#endif
		newdir.e2d_reclen = h2fs16(spacefree + dsize);
	} else {
#ifdef DIAGNOSTIC
		if (spacefree < newentrysize) {
			printf("ext2fs_direnter: compact2 %u %u",
			    (u_int)spacefree, (u_int)newentrysize);
			panic("ext2fs_direnter: compact2");
		}
#endif
		newdir.e2d_reclen = h2fs16(spacefree);
		ep->e2d_reclen = h2fs16(dsize);
		ep = (struct ext2fs_direct *)((char *)ep + dsize);
	}
	memcpy((void *)ep, (void *)&newdir, (u_int)newentrysize);
	error = VOP_BWRITE(bp->b_vp, bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	if (!error && ulr->ulr_endoff && ulr->ulr_endoff < ext2fs_size(dp))
		error = ext2fs_truncate(dvp, (off_t)ulr->ulr_endoff, IO_SYNC,
		    cnp->cn_cred);
	return (error);
}

/*
 * Remove a directory entry after a call to namei, using
 * the auxiliary results it provided. The entry
 * ulr_offset contains the offset into the directory of the
 * entry to be eliminated.  The ulr_count field contains the
 * size of the previous record in the directory.  If this
 * is 0, the first entry is being deleted, so we need only
 * zero the inode number to mark the entry as free.  If the
 * entry is not the first in the directory, we must reclaim
 * the space of the now empty record by adding the record size
 * to the size of the previous entry.
 */
int
ext2fs_dirremove(struct vnode *dvp, const struct ufs_lookup_results *ulr,
		 struct componentname *cnp)
{
	struct inode *dp;
	struct ext2fs_direct *ep;
	struct buf *bp;
	int error;

	dp = VTOI(dvp);

	if (ulr->ulr_count == 0) {
		/*
		 * First entry in block: set d_ino to zero.
		 */
		error = ext2fs_blkatoff(dvp, (off_t)ulr->ulr_offset,
		    (void *)&ep, &bp);
		if (error != 0)
			return (error);
		ep->e2d_ino = 0;
		error = VOP_BWRITE(bp->b_vp, bp);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
		return (error);
	}
	/*
	 * Collapse new free space into previous entry.
	 */
	error = ext2fs_blkatoff(dvp, (off_t)(ulr->ulr_offset - ulr->ulr_count),
	    (void *)&ep, &bp);
	if (error != 0)
		return (error);
	ep->e2d_reclen = h2fs16(fs2h16(ep->e2d_reclen) + ulr->ulr_reclen);
	error = VOP_BWRITE(bp->b_vp, bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	return (error);
}

/*
 * Rewrite an existing directory entry to point at the inode
 * supplied.  The parameters describing the directory entry are
 * set up by a call to namei.
 */
int
ext2fs_dirrewrite(struct inode *dp, const struct ufs_lookup_results *ulr,
    struct inode *ip, struct componentname *cnp)
{
	struct buf *bp;
	struct ext2fs_direct *ep;
	struct vnode *vdp = ITOV(dp);
	int error;

	error = ext2fs_blkatoff(vdp, (off_t)ulr->ulr_offset, (void *)&ep, &bp);
	if (error != 0)
		return (error);
	ep->e2d_ino = h2fs32(ip->i_number);
	if (ip->i_e2fs->e2fs.e2fs_rev > E2FS_REV0 &&
	    (ip->i_e2fs->e2fs.e2fs_features_incompat & EXT2F_INCOMPAT_FTYPE)) {
		ep->e2d_type = inot2ext2dt(IFTODT(ip->i_e2fs_mode));
	} else {
		ep->e2d_type = 0;
	}
	error = VOP_BWRITE(bp->b_vp, bp);
	dp->i_flag |= IN_CHANGE | IN_UPDATE;
	return (error);
}

/*
 * Check if a directory is empty or not.
 * Inode supplied must be locked.
 *
 * Using a struct dirtemplate here is not precisely
 * what we want, but better than using a struct ext2fs_direct.
 *
 * NB: does not handle corrupted directories.
 */
int
ext2fs_dirempty(struct inode *ip, ino_t parentino, kauth_cred_t cred)
{
	off_t off;
	struct ext2fs_dirtemplate dbuf;
	struct ext2fs_direct *dp = (struct ext2fs_direct *)&dbuf;
	int error, namlen;
	size_t count;

#define	MINDIRSIZ (sizeof (struct ext2fs_dirtemplate) / 2)

	for (off = 0; off < ext2fs_size(ip); off += fs2h16(dp->e2d_reclen)) {
		error = vn_rdwr(UIO_READ, ITOV(ip), (void *)dp, MINDIRSIZ, off,
		   UIO_SYSSPACE, IO_NODELOCKED, cred, &count, NULL);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (error || count != 0)
			return (0);
		/* avoid infinite loops */
		if (dp->e2d_reclen == 0)
			return (0);
		/* skip empty entries */
		if (dp->e2d_ino == 0)
			continue;
		/* accept only "." and ".." */
		namlen = dp->e2d_namlen;
		if (namlen > 2)
			return (0);
		if (dp->e2d_name[0] != '.')
			return (0);
		/*
		 * At this point namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (namlen == 1)
			continue;
		if (dp->e2d_name[1] == '.' && fs2h32(dp->e2d_ino) == parentino)
			continue;
		return (0);
	}
	return (1);
}

/*
 * Check if source directory is in the path of the target directory.
 * Target is supplied locked, source is unlocked.
 * The target is always vput before returning.
 */
int
ext2fs_checkpath(struct inode *source, struct inode *target,
	kauth_cred_t cred)
{
	struct vnode *vp;
	int error, rootino, namlen;
	struct ext2fs_dirtemplate dirbuf;
	uint32_t ino;

	vp = ITOV(target);
	if (target->i_number == source->i_number) {
		error = EEXIST;
		goto out;
	}
	rootino = UFS_ROOTINO;
	error = 0;
	if (target->i_number == rootino)
		goto out;

	for (;;) {
		if (vp->v_type != VDIR) {
			error = ENOTDIR;
			break;
		}
		error = vn_rdwr(UIO_READ, vp, (void *)&dirbuf,
			sizeof (struct ext2fs_dirtemplate), (off_t)0,
			UIO_SYSSPACE, IO_NODELOCKED, cred, (size_t *)0,
			NULL);
		if (error != 0)
			break;
		namlen = dirbuf.dotdot_namlen;
		if (namlen != 2 ||
			dirbuf.dotdot_name[0] != '.' ||
			dirbuf.dotdot_name[1] != '.') {
			error = ENOTDIR;
			break;
		}
		ino = fs2h32(dirbuf.dotdot_ino);
		if (ino == source->i_number) {
			error = EINVAL;
			break;
		}
		if (ino == rootino)
			break;
		vput(vp);
		error = VFS_VGET(vp->v_mount, ino, &vp);
		if (error != 0) {
			vp = NULL;
			break;
		}
	}

out:
	if (error == ENOTDIR) {
		printf("checkpath: .. not a directory\n");
		panic("checkpath");
	}
	if (vp != NULL)
		vput(vp);
	return (error);
}
