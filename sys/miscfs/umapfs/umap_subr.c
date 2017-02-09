/*	$NetBSD: umap_subr.c,v 1.29 2014/11/09 18:08:07 maxv Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
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
 *	from: Id: lofs_subr.c, v 1.11 1992/05/30 10:05:43 jsp Exp
 *	@(#)umap_subr.c	8.9 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: umap_subr.c,v 1.29 2014/11/09 18:08:07 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/kauth.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/umapfs/umap.h>

u_long umap_findid(u_long, u_long [][2], int);
int umap_node_alloc(struct mount *, struct vnode *,
				struct vnode **);

/*
 * umap_findid is called by various routines in umap_vnodeops.c to
 * find a user or group id in a map.
 */
u_long
umap_findid(u_long id, u_long map[][2], int nentries)
{
	int i;

	/* Find uid entry in map */
	i = 0;
	while ((i<nentries) && ((map[i][0]) != id))
		i++;

	if (i < nentries)
		return (map[i][1]);
	else
		return (-1);

}

/*
 * umap_reverse_findid is called by umap_getattr() in umap_vnodeops.c to
 * find a user or group id in a map, in reverse.
 */
u_long
umap_reverse_findid(u_long id, u_long map[][2], int nentries)
{
	int i;

	/* Find uid entry in map */
	i = 0;
	while ((i<nentries) && ((map[i][1]) != id))
		i++;

	if (i < nentries)
		return (map[i][0]);
	else
		return (-1);

}

/* umap_mapids maps all of the ids in a credential, both user and group. */

void
umap_mapids(struct mount *v_mount, kauth_cred_t credp)
{
	int i, unentries, gnentries;
	uid_t uid;
	gid_t gid;
	u_long (*usermap)[2], (*groupmap)[2];
	gid_t groups[NGROUPS];
	uint16_t ngroups;

	if (credp == NOCRED || credp == FSCRED)
		return;

	unentries =  MOUNTTOUMAPMOUNT(v_mount)->info_nentries;
	usermap =  MOUNTTOUMAPMOUNT(v_mount)->info_mapdata;
	gnentries =  MOUNTTOUMAPMOUNT(v_mount)->info_gnentries;
	groupmap =  MOUNTTOUMAPMOUNT(v_mount)->info_gmapdata;

	/* Find uid entry in map */

	uid = (uid_t) umap_findid(kauth_cred_geteuid(credp), usermap, unentries);

	if (uid != -1)
		kauth_cred_seteuid(credp, uid);
	else
		kauth_cred_seteuid(credp, (uid_t)NOBODY);

#if 1
	/* cr_gid is the same as cr_groups[0] in 4BSD, but not in NetBSD */

	/* Find gid entry in map */

	gid = (gid_t) umap_findid(kauth_cred_getegid(credp), groupmap, gnentries);

	if (gid != -1)
		kauth_cred_setegid(credp, gid);
	else
		kauth_cred_setegid(credp, NULLGROUP);
#endif

	/* Now we must map each of the set of groups in the cr_groups
		structure. */

	ngroups = kauth_cred_ngroups(credp);
	for (i = 0; i < ngroups; i++) {
		/* XXX elad: can't we just skip cases where gid == -1? */
		groups[i] = kauth_cred_group(credp, i);
		gid = (gid_t) umap_findid(groups[i],
					  groupmap, gnentries);
		if (gid != -1)
			groups[i] = gid;
		else
			groups[i] = NULLGROUP;
	}

	kauth_cred_setgroups(credp, groups, ngroups, -1, UIO_SYSSPACE);
}
