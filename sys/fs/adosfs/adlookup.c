/*	$NetBSD: adlookup.c,v 1.19 2014/02/07 15:29:21 hannken Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1996 Matthias Scheler
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: adlookup.c,v 1.19 2014/02/07 15:29:21 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <fs/adosfs/adosfs.h>

#ifdef ADOSFS_EXACTMATCH
#define strmatch(s1, l1, s2, l2, i) \
    ((l1) == (l2) && memcmp((s1), (s2), (l1)) == 0)
#else
#define strmatch(s1, l1, s2, l2, i) \
    ((l1) == (l2) && adoscaseequ((s1), (s2), (l1), (i)))
#endif

/*
 * adosfs lookup. enters with:
 * pvp (parent vnode) referenced and locked.
 * exit with:
 *	target vp referenced and locked.
 *	parent pvp locked.
 * special cases:
 *	pvp == vp, just ref pvp, pvp already holds a ref and lock from
 *	    caller, this will not occur with RENAME or CREATE.
 */
int
adosfs_lookup(void *v)
{
	struct vop_lookup_v2_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *sp = v;
	int nameiop, last, flags, error, nocache, i;
	struct componentname *cnp;
	struct vnode **vpp;	/* place to store result */
	struct anode *ap;	/* anode to find */
	struct vnode *vdp;	/* vnode of search dir */
	struct anode *adp;	/* anode of search dir */
	kauth_cred_t ucp;	/* lookup credentials */
	u_long bn, plen, hval;
	const u_char *pelt;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	cnp = sp->a_cnp;
	vdp = sp->a_dvp;
	adp = VTOA(vdp);
	vpp = sp->a_vpp;
	*vpp = NULL;
	ucp = cnp->cn_cred;
	nameiop = cnp->cn_nameiop;
	flags = cnp->cn_flags;
	last = flags & ISLASTCN;
	pelt = (const u_char *)cnp->cn_nameptr;
	plen = cnp->cn_namelen;
	nocache = 0;

	/*
	 * Check accessiblity of directory.
	 */
	if ((error = VOP_ACCESS(vdp, VEXEC, ucp)) != 0)
		return (error);

	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	/*
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if (cache_lookup(vdp, cnp->cn_nameptr, cnp->cn_namelen,
			 cnp->cn_nameiop, cnp->cn_flags, NULL, vpp)) {
		return *vpp == NULLVP ? ENOENT : 0;
	}

	/*
	 * fake a '.'
	 */
	if (plen == 1 && pelt[0] == '.') {
		/* see special cases in prologue. */
		*vpp = vdp;
		goto found;
	}
	/*
	 * fake a ".."
	 */
	if (flags & ISDOTDOT) {
		if (vdp->v_type == VDIR && (vdp->v_vflag & VV_ROOT))
			panic("adosfs .. attempted through root");
		/*
		 * cannot get `..' while `vdp' is locked
		 * e.g. procA holds lock on `..' and waits for `vdp'
		 * we wait for `..' and hold lock on `vdp'. deadlock.
		 * because `vdp' may have been achieved through symlink
		 * fancy detection code that decreases the race
		 * window size is not reasonably possible.
		 *
		 * basically unlock the parent, try and lock the child (..)
		 * if that fails relock the parent (ignoring error) and
		 * fail.  Otherwise we have the child (..), attempt to
		 * relock the parent.  If that fails unlock the child (..)
		 * and fail. Otherwise we have succeded.
		 *
		 */
		VOP_UNLOCK(vdp); /* race */
		error = VFS_VGET(vdp->v_mount, (ino_t)adp->pblock, vpp);
		vn_lock(vdp, LK_EXCLUSIVE | LK_RETRY);
		if (error) {
			*vpp = NULL;
			return (error);
		}
		goto found_lockdone;
	}

	/*
	 * hash the name and grab the first block in chain
	 * then walk the chain. if chain has not been fully
	 * walked before, track the count in `tabi'
	 */
	hval = adoshash(pelt, plen, adp->ntabent, IS_INTER(adp->amp));
	bn = adp->tab[hval];
	i = min(adp->tabi[hval], 0);
	while (bn != 0) {
		if ((error = VFS_VGET(vdp->v_mount, (ino_t)bn, vpp
				      )) != 0) {
#ifdef ADOSFS_DIAGNOSTIC
			printf("[aget] %d)", error);
#endif
			return(error);
		}
		ap = VTOA(*vpp);
		if (i <= 0) {
			if (--i < adp->tabi[hval])
				adp->tabi[hval] = i;
			/*
			 * last header in chain lock count down by
			 * negating it to positive
			 */
			if (ap->hashf == 0) {
#ifdef DEBUG
				if (i != adp->tabi[hval])
					panic("adlookup: wrong chain count");
#endif
				adp->tabi[hval] = -adp->tabi[hval];
			}
		}
		if (strmatch(pelt, plen, ap->name, strlen(ap->name),
		    IS_INTER(adp->amp)))
			goto found;
		bn = ap->hashf;
		vput(*vpp);
	}
	*vpp = NULL;
	/*
	 * not found
	 */
	if ((nameiop == CREATE || nameiop == RENAME) && last) {

		if (vdp->v_mount->mnt_flag & MNT_RDONLY)
			return (EROFS);

		if ((error = VOP_ACCESS(vdp, VWRITE, ucp)) != 0) {
#ifdef ADOSFS_DIAGNOSTIC
			printf("[VOP_ACCESS] %d)", error);
#endif
			return (error);
		}
#ifdef ADOSFS_DIAGNOSTIC
		printf("EJUSTRETURN)");
#endif
		return(EJUSTRETURN);
	}
	if (nameiop != CREATE)
		cache_enter(vdp, NULL, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);
#ifdef ADOSFS_DIAGNOSTIC
	printf("ENOENT)");
#endif
	return(ENOENT);

found:
	if (nameiop == DELETE && last)  {
		if ((error = VOP_ACCESS(vdp, VWRITE, ucp)) != 0) {
			if (vdp != *vpp)
				vput(*vpp);
			*vpp = NULL;
			return (error);
		}
		nocache = 1;
	}
	if (nameiop == RENAME && last) {
		if (vdp == *vpp)
			return(EISDIR);
		if ((error = VOP_ACCESS(vdp, VWRITE, ucp)) != 0) {
			vput(*vpp);
			*vpp = NULL;
			return (error);
		}
		nocache = 1;
	}
	if (vdp == *vpp)
		vref(vdp);
found_lockdone:
	if (*vpp != vdp)
		VOP_UNLOCK(*vpp);
	if (nocache == 0)
		cache_enter(vdp, *vpp, cnp->cn_nameptr, cnp->cn_namelen,
			    cnp->cn_flags);

#ifdef ADOSFS_DIAGNOSTIC
	printf("0)\n");
#endif
	return(0);
}
