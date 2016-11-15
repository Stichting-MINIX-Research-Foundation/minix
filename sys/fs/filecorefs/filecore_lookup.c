/*	$NetBSD: filecore_lookup.c,v 1.21 2014/10/04 13:27:24 hannken Exp $	*/

/*-
 * Copyright (c) 1989, 1993, 1994 The Regents of the University of California.
 * All rights reserved.
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
 *	filecore_lookup.c	1.1	1998/6/26
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
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
 *	filecore_lookup.c	1.1	1998/6/26
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filecore_lookup.c,v 1.21 2014/10/04 13:27:24 hannken Exp $");

#include <sys/param.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/systm.h>

#include <fs/filecorefs/filecore.h>
#include <fs/filecorefs/filecore_extern.h>
#include <fs/filecorefs/filecore_node.h>

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
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and iput
 * instead of two iputs.
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
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 *
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent inode unlocked.
 */
int
filecore_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	struct vnode *vdp;		/* vnode for directory being searched */
	struct filecore_node *dp;	/* inode for directory being searched */
	struct buf *bp;			/* a buffer of directory entries */
	struct filecore_direntry *de;
	int numdirpasses;		/* strategy for directory search */
	int error;
	u_short namelen;
	int res;
	const char *name;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	kauth_cred_t cred = cnp->cn_cred;
	int flags;
	int nameiop = cnp->cn_nameiop;
	int i, endsearch;

	flags = cnp->cn_flags;

	bp = NULL;
	*vpp = NULL;
	vdp = ap->a_dvp;
	dp = VTOI(vdp);

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

	name = cnp->cn_nameptr;
	namelen = cnp->cn_namelen;

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
	if (nameiop != LOOKUP || dp->i_diroff == 0 ||
	    dp->i_diroff >= FILECORE_MAXDIRENTS) {
		i = 0;
		numdirpasses = 1;
	} else {
		i = dp->i_diroff;
		numdirpasses = 2;
		namecache_count_2passes();
	}
	endsearch = FILECORE_MAXDIRENTS;

	if ((flags & ISDOTDOT) || (name[0] == '.' && namelen == 1))
		goto found;

	error = filecore_dbread(dp, &bp);
	if (error) {
		return error;
	}

	de = fcdirentry(bp->b_data, i);

searchloop:
	while (de->name[0] != 0 && i < endsearch) {
		/*
		 * Check for a name match.
		 */
		res = filecore_fncmp(de->name, name, namelen);

		if (res == 0)
			goto found;
		if (res < 0)
			goto notfound;

		i++;
		de++;
	}

notfound:
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		i = 0;
		de = fcdirentry(bp->b_data, i);
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL) {
#ifdef FILECORE_DEBUG_BR
			printf("brelse(%p) lo1\n", bp);
#endif
		brelse(bp, 0);
	}

	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_flags);
	return (nameiop == CREATE || nameiop == RENAME) ? EROFS : ENOENT;

found:
	if (numdirpasses == 2)
		namecache_count_pass2();

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if ((flags & ISLASTCN) && nameiop == LOOKUP)
		dp->i_diroff = i;

	if (name[0] == '.' && namelen == 1) {
		vref(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
		ino_t ino;

		if (flags & ISDOTDOT) {
			ino = filecore_getparent(dp);
		} else {
			ino = dp->i_dirent.addr | (i << FILECORE_INO_INDEX);
#ifdef FILECORE_DEBUG_BR
			printf("brelse(%p) lo4\n", bp);
#endif
			brelse(bp, 0);
		}
		error = vcache_get(vdp->v_mount, &ino, sizeof(ino), vpp);
		if (error)
			return error;
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
		    cnp->cn_flags);
	return 0;
}
