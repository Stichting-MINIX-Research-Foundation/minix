/*	$NetBSD: subr.c,v 1.6 2007/11/30 19:02:39 pooka Exp $	*/

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
#ifndef lint
__RCSID("$NetBSD: subr.c,v 1.6 2007/11/30 19:02:39 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>

#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <util.h>

#include "ninepuffs.h"
#include "nineproto.h"

void
qid2vattr(struct vattr *vap, const struct qid9p *qid)
{

	vap->va_fileid = qid->qidpath;
	vap->va_gen = qid->qidvers;
	if (qid->qidtype & P9PROTO_QID_TYPE_DIR)
		vap->va_type = VDIR;
	else
		vap->va_type = VREG;
}

static struct puffs_node *
makep9pnode(struct puffs_usermount *pu, p9pfid_t fid)
{
	struct p9pnode *p9n;
	struct puffs_node *pn;

	p9n = emalloc(sizeof(struct p9pnode));
	memset(p9n, 0, sizeof(struct p9pnode));
	p9n->fid_base = fid;
	LIST_INIT(&p9n->dir_openlist);

	pn = puffs_pn_new(pu, p9n);
	if (pn == NULL)
		abort();

	return pn;
}

struct puffs_node *
newp9pnode_va(struct puffs_usermount *pu, const struct vattr *va, p9pfid_t fid)
{
	struct puffs_node *pn;

	pn = makep9pnode(pu, fid);
	pn->pn_va = *va;

	return pn;
}

struct puffs_node *
newp9pnode_qid(struct puffs_usermount *pu, const struct qid9p *qid,
	p9pfid_t fid)
{
	struct puffs_node *pn;

	pn = makep9pnode(pu, fid);
	puffs_vattr_null(&pn->pn_va);
	qid2vattr(&pn->pn_va, qid);

	return pn;
}

/*
 * search list of fids, or if none is found, walk a fid for a new one
 * and issue dummy readdirs until we get the result we want
 */
int
getdfwithoffset(struct puffs_usermount *pu, struct p9pnode *p9n, off_t wantoff,
	struct dirfid **rfid)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct puffs9p *p9p = puffs_getspecific(pu);
	struct puffs_framebuf *pb;
	struct dirfid *dfp = NULL;
	p9ptag_t tag = NEXTTAG(p9p);
	off_t curoff, advance;
	uint32_t count;
	int rv;

	LIST_FOREACH(dfp, &p9n->dir_openlist, entries) {
		if (dfp->seekoff == wantoff) {
			LIST_REMOVE(dfp, entries);
			*rfid = dfp;
			return 0;
		}
	}

	/* didn't get off easy?  damn, do manual labour */
	pb = p9pbuf_makeout();
	dfp = ecalloc(1, sizeof(struct dirfid));
	dfp->fid = NEXTFID(p9p);
	rv = proto_cc_open(pu, p9n->fid_base, dfp->fid, P9PROTO_OMODE_READ);
	if (rv)
		goto out;

	for (curoff = 0;;) {
		advance = wantoff - curoff;

		tag = NEXTTAG(p9p);
		p9pbuf_put_1(pb, P9PROTO_T_READ);  
		p9pbuf_put_2(pb, tag);
		p9pbuf_put_4(pb, dfp->fid);       
		p9pbuf_put_8(pb, 0);
		p9pbuf_put_4(pb, advance);       
		GETRESPONSE(pb);

		if (p9pbuf_get_type(pb) != P9PROTO_R_READ) {
			rv = EPROTO;
			goto out;
		}

		/*
		 * Check how many bytes we got.  If we got the amount we
		 * wanted, we are at the correct position.  If we got
		 * zero bytes, either the directory doesn't "support" the
		 * seek offset we want (someone has probably inserted an
		 * entry meantime) or we at the end of directory.  Either
		 * way, let the upper layer deal with it.
		 */
		p9pbuf_get_4(pb, &count);
		curoff += count;
		if (count == advance || count == 0)
			break;

		p9pbuf_recycleout(pb);
	}
	puffs_framebuf_destroy(pb);

	dfp->seekoff = curoff;
	*rfid = dfp;
	return 0;

 out:
	puffs_framebuf_destroy(pb);
	free(dfp);
	return rv;
}

void
releasedf(struct puffs_usermount *pu, struct dirfid *dfp)
{

	proto_cc_clunkfid(pu, dfp->fid, 0);
	free(dfp);
}

void
storedf(struct p9pnode *p9n, struct dirfid *dfp)
{

	LIST_INSERT_HEAD(&p9n->dir_openlist, dfp, entries);
}

void
nukealldf(struct puffs_usermount *pu, struct p9pnode *p9n)
{
	struct dirfid *dfp;

	while ((dfp = LIST_FIRST(&p9n->dir_openlist)) != NULL) {
		LIST_REMOVE(dfp, entries);
		releasedf(pu, dfp);
	}
}
