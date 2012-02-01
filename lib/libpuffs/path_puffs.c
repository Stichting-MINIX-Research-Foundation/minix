/* 
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
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
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/hash.h>

#include <assert.h>
#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <string.h>

#include "puffs.h"
#include "puffs_priv.h"


/*
 * Generic routines for pathbuilding code
 */

int
puffs_path_pcnbuild(struct puffs_usermount *pu, struct puffs_cn *pcn,
	puffs_cookie_t parent)
{
	struct puffs_node *pn_parent = PU_CMAP(pu, parent);
	struct puffs_cn pcn_orig;
	struct puffs_pathobj po;
	int rv;

	assert(pn_parent->pn_po.po_path != NULL);
	assert(pu->pu_flags & PUFFS_FLAG_BUILDPATH);
	pcn_orig = *pcn;

	if (pu->pu_pathtransform) {
		rv = pu->pu_pathtransform(pu, &pn_parent->pn_po, pcn, &po);
		if (rv)
			return rv;
	} else {
		po.po_path = pcn->pcn_name;
		po.po_len = pcn->pcn_namelen;
	}

	if (pu->pu_namemod) {
		rv = pu->pu_namemod(pu, &pn_parent->pn_po, pcn);
		if (rv)
			return rv;
	}

	rv = pu->pu_pathbuild(pu, &pn_parent->pn_po, &po, 0,
	    &pcn->pcn_po_full);
	puffs_path_buildhash(pu, &pcn->pcn_po_full);

	if (pu->pu_pathtransform)
		pu->pu_pathfree(pu, &po);

	if (pu->pu_namemod && rv)
		*pcn = pcn_orig;

	return rv;
}

/*
 * substitute all (child) patch prefixes.  called from nodewalk, which
 * in turn is called from rename
 */
void *
puffs_path_prefixadj(struct puffs_usermount *pu, struct puffs_node *pn,
	void *arg)
{
	struct puffs_pathinfo *pi = arg;
	struct puffs_pathobj localpo;
	struct puffs_pathobj oldpo;
	int rv;

	/* can't be a path prefix */
	if (pn->pn_po.po_len < pi->pi_old->po_len)
		return NULL;

	if (pu->pu_pathcmp(pu, &pn->pn_po, pi->pi_old, pi->pi_old->po_len, 1))
		return NULL;

	/* otherwise we'd have two nodes with an equal path */
	assert(pn->pn_po.po_len > pi->pi_old->po_len);

	/* found a matching prefix */
	rv = pu->pu_pathbuild(pu, pi->pi_new, &pn->pn_po,
	    pi->pi_old->po_len, &localpo);
	/*
	 * XXX: technically we shouldn't fail, but this is the only
	 * sensible thing to do here.  If the buildpath routine fails,
	 * we will have paths in an inconsistent state.  Should fix this,
	 * either by having two separate passes or by doing other tricks
	 * to make an invalid path with BUILDPATHS acceptable.
	 */
	if (rv != 0)
		abort();

	/* adjust hash sum */
	puffs_path_buildhash(pu, &localpo);

	/* out with the old and in with the new */
	oldpo = pn->pn_po;
	pn->pn_po = localpo;
	pu->pu_pathfree(pu, &oldpo);

	/* continue the walk */
	return NULL;
}

/*
 * called from nodewalk, checks for exact match
 */
void *
puffs_path_walkcmp(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{
	struct puffs_pathobj *po = arg;
	struct puffs_pathobj po2;

	if (po->po_len != PNPLEN(pn))
		return NULL;

	/*
	 * If hashing and the hash doesn't match, we know this is
	 * definitely not a match.  Otherwise check for collisions.
	 */
	if (pu->pu_flags & PUFFS_FLAG_HASHPATH)
		if (pn->pn_po.po_hash != po->po_hash)
			return NULL;

	po2.po_path = PNPATH(pn);
	po2.po_len = PNPLEN(pn);

	if (pu->pu_pathcmp(pu, po, &po2, PNPLEN(pn), 0) == 0)
		return pn;
	return NULL;
}

/*
 * Hash sum building routine.  Use string hash if the buildpath routine
 * is the standard one, otherwise use binary hashes.  A bit whimsical
 * way to choose the routine, but the binary works for strings also,
 * so don't sweat it.
 */
void
puffs_path_buildhash(struct puffs_usermount *pu, struct puffs_pathobj *po)
{

	if ((pu->pu_flags & PUFFS_FLAG_HASHPATH) == 0)
		return;

	if (pu->pu_pathbuild == puffs_stdpath_buildpath)
		po->po_hash = hash32_strn(po->po_path, po->po_len,
		    HASH32_STR_INIT);
	else
		po->po_hash = hash32_buf(po->po_path, po->po_len,
		    HASH32_BUF_INIT);
}

/*
 * Routines provided to file systems which consider a path a tuple of
 * strings and / the component separator.
 */

/*ARGSUSED*/
int
puffs_stdpath_cmppath(struct puffs_usermount *pu, struct puffs_pathobj *c1,
	struct puffs_pathobj *c2, size_t clen, int checkprefix)
{
	char *p;
	int rv;

	rv = strncmp(c1->po_path, c2->po_path, clen);
	if (rv)
		return 1;

	if (checkprefix == 0)
		return 0;

	/* sanity for next step */
	if (!(c1->po_len > c2->po_len))
		return 1;

	/* check if it's really a complete path prefix */
	p = c1->po_path;
	if ((*(p + clen)) != '/')
		return 1;

	return 0;
}

/*ARGSUSED*/
int
puffs_stdpath_buildpath(struct puffs_usermount *pu,
	const struct puffs_pathobj *po_pre, const struct puffs_pathobj *po_comp,
	size_t offset, struct puffs_pathobj *newpath)
{
	char *path, *pcomp;
	size_t plen, complen;
	size_t prelen;
	int isdotdot;

	complen = po_comp->po_len - offset;

	/* seek to correct place & remove all leading '/' from component */
	pcomp = po_comp->po_path;
	pcomp += offset;
	while (*pcomp == '/') {
		pcomp++;
		complen--;
	}

	/* todotdot or nottodotdot */
	if (complen == 2 && strcmp(pcomp, "..") == 0)
		isdotdot = 1;
	else
		isdotdot = 0;

	/*
	 * Strip trailing components from the preceending component.
	 * This is an issue only for the root node, which we might want
	 * to be at path "/" for some file systems.
	 */
	prelen = po_pre->po_len;
	while (prelen > 0 && *((char *)po_pre->po_path + (prelen-1)) == '/') {
		assert(isdotdot == 0);
		prelen--;
	}

	if (isdotdot) {
		char *slash; /* sweet char of mine */
		
		slash = strrchr(po_pre->po_path, '/');
		assert(slash != NULL);

		plen = slash - (char *)po_pre->po_path;

		/*
		 * As the converse to not stripping the initial "/" above,
		 * don't nuke it here either.
		 */
		if (plen == 0)
			plen++;

		path = malloc(plen + 1);
		if (path == NULL)
			return errno;

		strlcpy(path, po_pre->po_path, plen+1);
	} else {
		/* + '/' + '\0' */
		plen = prelen + 1 + complen;
		path = malloc(plen + 1);
		if (path == NULL)
			return errno;

		strlcpy(path, po_pre->po_path, prelen+1);
		strcat(path, "/");
		strncat(path, pcomp, complen);
	}

	newpath->po_path = path;
	newpath->po_len = plen;

	return 0;
}

/*ARGSUSED*/
void
puffs_stdpath_freepath(struct puffs_usermount *pu, struct puffs_pathobj *po)
{

	free(po->po_path);
}
