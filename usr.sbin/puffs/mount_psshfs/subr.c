/*      $NetBSD: subr.c,v 1.51 2012/11/04 22:46:08 christos Exp $        */

/*
 * Copyright (c) 2006  Antti Kantee.  All Rights Reserved.
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
#ifndef lint
__RCSID("$NetBSD: subr.c,v 1.51 2012/11/04 22:46:08 christos Exp $");
#endif /* !lint */

#include <stdio.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <util.h>

#include "psshfs.h"
#include "sftp_proto.h"

static void
freedircache(struct psshfs_dir *base, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		free(base[i].entryname);
		base[i].entryname = NULL;
	}

	free(base);
}

#define ENTRYCHUNK 16
static void
allocdirs(struct psshfs_node *psn)
{
	size_t oldtot = psn->denttot;

	psn->denttot += ENTRYCHUNK;
	psn->dir = erealloc(psn->dir,
	    psn->denttot * sizeof(struct psshfs_dir));
	memset(psn->dir + oldtot, 0, ENTRYCHUNK * sizeof(struct psshfs_dir));
}

static void
setpnva(struct puffs_usermount *pu, struct puffs_node *pn,
	const struct vattr *vap)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct psshfs_node *psn = pn->pn_data;
	struct vattr modva;

	/*
	 * Check if the file was modified from below us.
	 * If so, invalidate page cache.  This is the only
	 * sensible place we can do this in.
	 */
	if (pn->pn_va.va_mtime.tv_sec != PUFFS_VNOVAL)
		if (pn->pn_va.va_mtime.tv_sec != vap->va_mtime.tv_sec
		    && pn->pn_va.va_type == VREG)
			puffs_inval_pagecache_node(pu, pn);

	modva = *vap;
	if (pctx->domangleuid && modva.va_uid == pctx->mangleuid)
		modva.va_uid = pctx->myuid;
	if (pctx->domanglegid && modva.va_gid == pctx->manglegid)
		modva.va_gid = pctx->mygid;

	puffs_setvattr(&pn->pn_va, &modva);
	psn->attrread = time(NULL);
}

struct psshfs_dir *
lookup(struct psshfs_dir *bdir, size_t ndir, const char *name)
{
	struct psshfs_dir *test;
	size_t i;

	for (i = 0; i < ndir; i++) {
		test = &bdir[i];
		if (test->valid != 1)
			continue;
		if (strcmp(test->entryname, name) == 0)
			return test;
	}

	return NULL;
}

static struct psshfs_dir *
lookup_by_entry(struct psshfs_dir *bdir, size_t ndir, struct puffs_node *entry)
{
	struct psshfs_dir *test;
	size_t i;

	for (i = 0; i < ndir; i++) {
		test = &bdir[i];
		if (test->valid != 1)
			continue;
		if (test->entry == entry)
			return test;
	}

	return NULL;
}


void
closehandles(struct puffs_usermount *pu, struct psshfs_node *psn, int which)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct puffs_framebuf *pb1, *pb2;
	uint32_t reqid;

	if (psn->fhand_r && (which & HANDLE_READ)) {
		assert(psn->lazyopen_r == NULL);

		pb1 = psbuf_makeout();
		reqid = NEXTREQ(pctx);
		psbuf_req_data(pb1, SSH_FXP_CLOSE, reqid,
		    psn->fhand_r, psn->fhand_r_len);
		puffs_framev_enqueue_justsend(pu, pctx->sshfd_data, pb1, 1, 0);
		free(psn->fhand_r);
		psn->fhand_r = NULL;
	}

	if (psn->fhand_w && (which & HANDLE_WRITE)) {
		assert(psn->lazyopen_w == NULL);

		pb2 = psbuf_makeout();
		reqid = NEXTREQ(pctx);
		psbuf_req_data(pb2, SSH_FXP_CLOSE, reqid,
		    psn->fhand_w, psn->fhand_w_len);
		puffs_framev_enqueue_justsend(pu, pctx->sshfd_data, pb2, 1, 0);
		free(psn->fhand_w);
		psn->fhand_w = NULL;
	}

	psn->stat |= PSN_HANDLECLOSE;
}

void
lazyopen_rresp(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	void *arg, int error)
{
	struct psshfs_node *psn = arg;

	/* XXX: this is not enough */
	if (psn->stat & PSN_RECLAIMED) {
		error = ENOENT;
		goto moreout;
	}
	if (error)
		goto out;

	error = psbuf_expect_handle(pb, &psn->fhand_r, &psn->fhand_r_len);

 out:
	psn->lazyopen_err_r = error;
	psn->lazyopen_r = NULL;
	if (error)
		psn->stat &= ~PSN_DOLAZY_R;
	if (psn->stat & PSN_HANDLECLOSE && (psn->stat & PSN_LAZYWAIT_R) == 0)
		closehandles(pu, psn, HANDLE_READ);
 moreout:
	puffs_framebuf_destroy(pb);
}

void
lazyopen_wresp(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	void *arg, int error)
{
	struct psshfs_node *psn = arg;

	/* XXX: this is not enough */
	if (psn->stat & PSN_RECLAIMED) {
		error = ENOENT;
		goto moreout;
	}
	if (error)
		goto out;

	error = psbuf_expect_handle(pb, &psn->fhand_w, &psn->fhand_w_len);

 out:
	psn->lazyopen_err_w = error;
	psn->lazyopen_w = NULL;
	if (error)
		psn->stat &= ~PSN_DOLAZY_W;
	if (psn->stat & PSN_HANDLECLOSE && (psn->stat & PSN_LAZYWAIT_W) == 0)
		closehandles(pu, psn, HANDLE_WRITE);
 moreout:
	puffs_framebuf_destroy(pb);
}

struct readdirattr {
	struct psshfs_node *psn;
	int idx;
	char entryname[MAXPATHLEN+1];
};

int
getpathattr(struct puffs_usermount *pu, const char *path, struct vattr *vap)
{
	PSSHFSAUTOVAR(pu);

	psbuf_req_str(pb, SSH_FXP_LSTAT, reqid, path);
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_attrs(pb, vap);

 out:
	PSSHFSRETURN(rv);
}

int
getnodeattr(struct puffs_usermount *pu, struct puffs_node *pn, const char *path)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct psshfs_node *psn = pn->pn_data;
	struct vattr va;
	int rv;

	if (!psn->attrread || REFRESHTIMEOUT(pctx, time(NULL)-psn->attrread)) {
		rv = getpathattr(pu, path ? path : PNPATH(pn), &va);
		if (rv)
			return rv;

		setpnva(pu, pn, &va);
	}

	return 0;
}

int
sftp_readdir(struct puffs_usermount *pu, struct psshfs_ctx *pctx,
	struct puffs_node *pn)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct psshfs_node *psn = pn->pn_data;
	struct psshfs_dir *olddir, *testd;
	struct puffs_framebuf *pb;
	uint32_t reqid = NEXTREQ(pctx);
	uint32_t count, dhandlen;
	char *dhand = NULL;
	size_t nent;
	char *longname = NULL;
	size_t idx;
	int rv;

	assert(pn->pn_va.va_type == VDIR);
	idx = 0;
	olddir = psn->dir;
	nent = psn->dentnext;

	if (psn->dir && psn->dentread
	    && !REFRESHTIMEOUT(pctx, time(NULL) - psn->dentread))
		return 0;

	if (psn->dentread) {
		if ((rv = puffs_inval_namecache_dir(pu, pn)))
			warn("readdir: dcache inval fail %p", pn);
	}

	pb = psbuf_makeout();
	psbuf_req_str(pb, SSH_FXP_OPENDIR, reqid, PNPATH(pn));
	if (puffs_framev_enqueue_cc(pcc, pctx->sshfd, pb, 0) == -1) {
		rv = errno;
		goto wayout;
	}
	rv = psbuf_expect_handle(pb, &dhand, &dhandlen);
	if (rv)
		goto wayout;

	/*
	 * Well, the following is O(n^2), so feel free to improve if it
	 * gets too taxing on your system.
	 */

	/*
	 * note: for the "getattr in batch" to work, this must be before
	 * the attribute-getting.  Otherwise times for first entries in
	 * large directories might expire before the directory itself and
	 * result in one-by-one attribute fetching.
	 */
	psn->dentread = time(NULL);

	psn->dentnext = 0;
	psn->denttot = 0;
	psn->dir = NULL;

	for (;;) {
		reqid = NEXTREQ(pctx);
		psbuf_recycleout(pb);
		psbuf_req_data(pb, SSH_FXP_READDIR, reqid, dhand, dhandlen);
		GETRESPONSE(pb, pctx->sshfd);

		/* check for EOF */
		if (psbuf_get_type(pb) == SSH_FXP_STATUS) {
			rv = psbuf_expect_status(pb);
			goto out;
		}
		rv = psbuf_expect_name(pb, &count);
		if (rv)
			goto out;

		for (; count--; idx++) {
			if (idx == psn->denttot)
				allocdirs(psn);
			if ((rv = psbuf_get_str(pb,
			    &psn->dir[idx].entryname, NULL)))
				goto out;
			if ((rv = psbuf_get_str(pb, &longname, NULL)) != 0)
				goto out;
			if ((rv = psbuf_get_vattr(pb, &psn->dir[idx].va)) != 0)
				goto out;
			if (sscanf(longname, "%*s%d",
			    &psn->dir[idx].va.va_nlink) != 1) {
				rv = EPROTO;
				goto out;
			}
			free(longname);
			longname = NULL;
			
			/*
			 * In case of DOT, copy the attributes (mostly
			 * because we want the link count for the root dir).
			 */
			if (strcmp(psn->dir[idx].entryname, ".") == 0) {
				setpnva(pu, pn, &psn->dir[idx].va);
			}

			/*
			 * Check if we already have a psshfs_dir for the
			 * name we are processing.  If so, use the old one.
			 * If not, create a new one
			 */
			testd = lookup(olddir, nent, psn->dir[idx].entryname);
			if (testd) {
				psn->dir[idx].entry = testd->entry;
				/*
				 * Has entry.  Update attributes to what
				 * we just got from the server.
				 */
				if (testd->entry) {
					setpnva(pu, testd->entry,
					    &psn->dir[idx].va);
					psn->dir[idx].va.va_fileid
					    = testd->entry->pn_va.va_fileid;

				/*
				 * No entry.  This can happen in two cases:
				 * 1) the file was created "behind our back"
				 *    on the server
				 * 2) we do two readdirs before we instantiate
				 *    the node (or run with -t 0).
				 *
				 * Cache attributes from the server in
				 * case we want to instantiate this node
				 * soon.  Also preserve the old inode number
				 * which was given when the dirent was created.
				 */
				} else {
					psn->dir[idx].va.va_fileid
					    = testd->va.va_fileid;
					testd->va = psn->dir[idx].va;
				}

			/* No previous entry?  Initialize this one. */
			} else {
				psn->dir[idx].entry = NULL;
				psn->dir[idx].va.va_fileid = pctx->nextino++;
			}
			psn->dir[idx].attrread = psn->dentread;
			psn->dir[idx].valid = 1;
		}
	}

 out:
	/* XXX: rv */
	psn->dentnext = idx;
	freedircache(olddir, nent);

	reqid = NEXTREQ(pctx);
	psbuf_recycleout(pb);
	psbuf_req_data(pb, SSH_FXP_CLOSE, reqid, dhand, dhandlen);
	puffs_framev_enqueue_justsend(pu, pctx->sshfd, pb, 1, 0);
	free(dhand);
	free(longname);

	return rv;

 wayout:
	free(dhand);
	PSSHFSRETURN(rv);
}

struct puffs_node *
makenode(struct puffs_usermount *pu, struct puffs_node *parent,
	const struct psshfs_dir *pd, const struct vattr *vap)
{
	struct psshfs_node *psn_parent = parent->pn_data;
	struct psshfs_node *psn;
	struct puffs_node *pn;

	psn = emalloc(sizeof(struct psshfs_node));
	memset(psn, 0, sizeof(struct psshfs_node));

	pn = puffs_pn_new(pu, psn);
	if (!pn) {
		free(psn);
		return NULL;
	}
	setpnva(pu, pn, &pd->va);
	setpnva(pu, pn, vap);
	psn->attrread = pd->attrread;

	psn->parent = parent;
	psn_parent->childcount++;

	TAILQ_INIT(&psn->pw);

	return pn;
}

struct puffs_node *
allocnode(struct puffs_usermount *pu, struct puffs_node *parent,
	const char *entryname, const struct vattr *vap)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct psshfs_dir *pd;
	struct puffs_node *pn;

	pd = direnter(parent, entryname);

	pd->va.va_fileid = pctx->nextino++;
	if (vap->va_type == VDIR) {
		pd->va.va_nlink = 2;
		parent->pn_va.va_nlink++;
	} else {
		pd->va.va_nlink = 1;
	}

	pn = makenode(pu, parent, pd, vap);
	if (pn) {
		pd->va.va_fileid = pn->pn_va.va_fileid;
		pd->entry = pn;
	}

	return pn;
}

struct psshfs_dir *
direnter(struct puffs_node *parent, const char *entryname)
{
	struct psshfs_node *psn_parent = parent->pn_data;
	struct psshfs_dir *pd;
	int i;

	/* create directory entry */
	if (psn_parent->denttot == psn_parent->dentnext)
		allocdirs(psn_parent);

	i = psn_parent->dentnext;
	pd = &psn_parent->dir[i];
	pd->entryname = estrdup(entryname);
	pd->valid = 1;
	pd->attrread = 0;
	puffs_vattr_null(&pd->va);
	psn_parent->dentnext++;

	return pd;
}

void
doreclaim(struct puffs_node *pn)
{
	struct psshfs_node *psn = pn->pn_data;
	struct psshfs_node *psn_parent;
	struct psshfs_dir *dent;

	psn_parent = psn->parent->pn_data;
	psn_parent->childcount--;

	/*
	 * Null out entry from directory.  Do not treat a missing entry
	 * as an invariant error, since the node might be removed from
	 * under us, and we might do a readdir before the reclaim resulting
	 * in no directory entry in the parent directory.
	 */
	dent = lookup_by_entry(psn_parent->dir, psn_parent->dentnext, pn);
	if (dent)
		dent->entry = NULL;

	if (pn->pn_va.va_type == VDIR) {
		freedircache(psn->dir, psn->dentnext);
		psn->denttot = psn->dentnext = 0;
	}
	if (psn->symlink)
		free(psn->symlink);

	puffs_pn_put(pn);
}

void
nukenode(struct puffs_node *node, const char *entryname, int reclaim)
{
	struct psshfs_node *psn, *psn_parent;
	struct psshfs_dir *pd;

	psn = node->pn_data;
	psn_parent = psn->parent->pn_data;
	pd = lookup(psn_parent->dir, psn_parent->dentnext, entryname);
	assert(pd != NULL);
	pd->valid = 0;
	free(pd->entryname);
	pd->entryname = NULL;

	if (node->pn_va.va_type == VDIR)
		psn->parent->pn_va.va_nlink--;

	if (reclaim)
		doreclaim(node);
}
