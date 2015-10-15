/*	$NetBSD: cd9660_lookup.c,v 1.30 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	from: @(#)ufs_lookup.c	7.33 (Berkeley) 5/19/91
 *
 *	@(#)cd9660_lookup.c	8.5 (Berkeley) 5/27/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cd9660_lookup.c,v 1.30 2015/03/28 19:24:05 maxv Exp $");

#include <sys/param.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/kauth.h>

#include <fs/cd9660/iso.h>
#include <fs/cd9660/cd9660_extern.h>
#include <fs/cd9660/cd9660_node.h>
#include <fs/cd9660/iso_rrip.h>
#include <fs/cd9660/cd9660_rrip.h>
#include <fs/cd9660/cd9660_mount.h>

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If the target of the pathname exists, lookup returns both the target
 * and its parent directory locked.
 * When creating or renaming, the target may * not be ".".
 * When deleting , the target may be "."., but the caller must check
 * to ensure it does an vrele and vput instead of two vputs.
 *
 * Overall outline of ufs_lookup:
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
 *	if at end of path and rewriting (RENAME), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 */
int
cd9660_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vdp;		/* vnode for directory being searched */
	struct iso_node *dp;		/* inode for directory being searched */
	struct iso_mnt *imp;		/* file system that directory is in */
	struct buf *bp;			/* a buffer of directory entries */
	struct iso_directory_record *ep = NULL;
					/* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	int saveoffset = -1;		/* offset of last directory entry in dir */
	int numdirpasses;		/* strategy for directory search */
	doff_t endsearch;		/* offset to end directory search */
	struct vnode *tdp;		/* returned by vcache_get */
	u_long bmask;			/* block offset mask */
	int error;
	ino_t ino = 0;
	int reclen;
	u_short namelen;
	char altname[ISO_MAXNAMLEN];
	int res;
	int assoc, len;
	const char *name;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	kauth_cred_t cred = cnp->cn_cred;
	int flags;
	int nameiop = cnp->cn_nameiop;

	flags = cnp->cn_flags;

	bp = NULL;
	*vpp = NULL;
	vdp = ap->a_dvp;
	dp = VTOI(vdp);
	imp = dp->i_mnt;

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

	len = cnp->cn_namelen;
	name = cnp->cn_nameptr;
	/*
	 * A leading `=' means, we are looking for an associated file
	 */
	assoc = (imp->iso_ftype != ISO_FTYPE_RRIP && *name == ASSOCCHAR);
	if (assoc) {
		len--;
		name++;
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
	bmask = imp->im_bmask;
	if (nameiop != LOOKUP || dp->i_diroff == 0 ||
	    dp->i_diroff > dp->i_size) {
		entryoffsetinblock = 0;
		dp->i_offset = 0;
		numdirpasses = 1;
	} else {
		dp->i_offset = dp->i_diroff;
		if ((entryoffsetinblock = dp->i_offset & bmask) &&
		    (error = cd9660_blkatoff(vdp, (off_t)dp->i_offset, NULL,
		    &bp)))
				return (error);
		numdirpasses = 2;
		namecache_count_2passes();
	}
	endsearch = dp->i_size;

searchloop:
	while (dp->i_offset < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		 */
		if ((dp->i_offset & bmask) == 0) {
			if (bp != NULL)
				brelse(bp, 0);
			error = cd9660_blkatoff(vdp, (off_t)dp->i_offset,
					     NULL, &bp);
			if (error)
				return (error);
			entryoffsetinblock = 0;
		}
		/*
		 * Get pointer to next entry.
		 */
		KASSERT(bp != NULL);
		ep = (struct iso_directory_record *)
			((char *)bp->b_data + entryoffsetinblock);

		reclen = isonum_711(ep->length);
		if (reclen == 0) {
			/* skip to next block, if any */
			dp->i_offset =
			    (dp->i_offset & ~bmask) + imp->logical_block_size;
			continue;
		}

		if (reclen < ISO_DIRECTORY_RECORD_SIZE)
			/* illegal entry, stop */
			break;

		if (entryoffsetinblock + reclen > imp->logical_block_size)
			/* entries are not allowed to cross boundaries */
			break;

		namelen = isonum_711(ep->name_len);

		if (reclen < ISO_DIRECTORY_RECORD_SIZE + namelen)
			/* illegal entry, stop */
			break;

		/*
		 * Check for a name match.
		 */
		switch (imp->iso_ftype) {
		default:
			if ((!(isonum_711(ep->flags)&4)) == !assoc) {
				if ((len == 1
				     && *name == '.')
				    || (flags & ISDOTDOT)) {
					if (namelen == 1
					    && ep->name[0] == ((flags & ISDOTDOT) ? 1 : 0)) {
						/*
						 * Save directory entry's inode number and
						 * release directory buffer.
						 */
						dp->i_ino = isodirino(ep, imp);
						goto found;
					}
					if (namelen != 1
					    || ep->name[0] != 0)
						goto notfound;
				} else if (!(res = isofncmp(name,len,
						   ep->name,namelen,
						   imp->im_joliet_level))) {
					if (isonum_711(ep->flags)&2)
						ino = isodirino(ep, imp);
					else
						ino = dbtob(bp->b_blkno)
							+ entryoffsetinblock;
					saveoffset = dp->i_offset;
				} else if (ino)
					goto foundino;
#ifdef	NOSORTBUG	/* On some CDs directory entries are not sorted correctly */
				else if (res < 0)
					goto notfound;
				else if (res > 0 && numdirpasses == 2)
					numdirpasses++;
#endif
			}
			break;
		case ISO_FTYPE_RRIP:
			if (isonum_711(ep->flags)&2)
				ino = isodirino(ep, imp);
			else
				ino = dbtob(bp->b_blkno) + entryoffsetinblock;
			dp->i_ino = ino;
			cd9660_rrip_getname(ep,altname,&namelen,&dp->i_ino,imp);
			if (namelen == cnp->cn_namelen) {
				if (imp->im_flags & ISOFSMNT_RRCASEINS) {
					if (strncasecmp(name, altname, namelen) == 0)
						goto found;
				} else {
					if (memcmp(name, altname, namelen) == 0)
						goto found;
				}
			}
			ino = 0;
			break;
		}
		dp->i_offset += reclen;
		entryoffsetinblock += reclen;
	}
	if (ino) {
foundino:
		dp->i_ino = ino;
		if (saveoffset != dp->i_offset) {
			if (cd9660_lblkno(imp, dp->i_offset) !=
			    cd9660_lblkno(imp, saveoffset)) {
				if (bp != NULL)
					brelse(bp, 0);
				if ((error = cd9660_blkatoff(vdp,
					    (off_t)saveoffset, NULL, &bp)) != 0)
					return (error);
			}
			entryoffsetinblock = saveoffset & bmask;
			ep = (struct iso_directory_record *)
				((char *)bp->b_data + entryoffsetinblock);
			dp->i_offset = saveoffset;
		}
		goto found;
	}
notfound:
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		dp->i_offset = 0;
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		brelse(bp, 0);

	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_flags);
	return (nameiop == CREATE || nameiop == RENAME) ? EROFS : ENOENT;

found:
	if (numdirpasses == 2)
		namecache_count_pass2();
	brelse(bp, 0);

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if ((flags & ISLASTCN) && nameiop == LOOKUP)
		dp->i_diroff = dp->i_offset;

	if (dp->i_number == dp->i_ino) {
		vref(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		error = vcache_get(vdp->v_mount,
		    &dp->i_ino, sizeof(dp->i_ino), &tdp);
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
 * Return buffer with the contents of block "offset" from the beginning of
 * directory "ip".  If "res" is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
int
cd9660_blkatoff(struct vnode *vp, off_t offset, char **res, struct buf **bpp)
{
	struct iso_node *ip;
	struct iso_mnt *imp;
	struct vnode *devvp;
	struct buf *bp;
	daddr_t lbn;
	int bsize, error;

	ip = VTOI(vp);
	imp = ip->i_mnt;
	lbn = cd9660_lblkno(imp, offset);
	bsize = cd9660_blksize(imp, ip, lbn);

	if ((error = VOP_BMAP(vp, lbn, &devvp, &lbn, NULL)) != 0) {
		*bpp = NULL;
		return error;
	}
	if ((error = bread(devvp, lbn, bsize, 0, &bp)) != 0) {
		*bpp = NULL;
		return (error);
	}
	if (res)
		*res = (char *)bp->b_data + cd9660_blkoff(imp, offset);
	*bpp = bp;
	return (0);
}
