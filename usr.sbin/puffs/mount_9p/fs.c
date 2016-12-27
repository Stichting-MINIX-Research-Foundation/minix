/*	$NetBSD: fs.c,v 1.9 2011/06/22 04:03:23 mrg Exp $	*/

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
__RCSID("$NetBSD: fs.c,v 1.9 2011/06/22 04:03:23 mrg Exp $");
#endif /* !lint */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <puffs.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ninepuffs.h"
#include "nineproto.h"

#define DO_IO(fname, a1, a2, a3, a4, rv)				\
	puffs_framebuf_seekset(a2, 0);					\
	*(a4) = 0;							\
	rv = fname(a1, a2, a3, a4);					\
	if (rv) errx(1, "p9p_handshake io failed %d, %d", rv, *a4) 

struct puffs_node *
p9p_handshake(struct puffs_usermount *pu,
	const char *username, const char *path)
{
	struct puffs9p *p9p = puffs_getspecific(pu);
	struct puffs_framebuf *pb;
	struct puffs_node *pn;
	struct vattr rootva;
	uint32_t maxreq;
	uint16_t dummy;
	p9ptag_t tagid, rtagid;
	p9pfid_t curfid;
	const char *p;
	uint8_t type;
	int rv, done, x = 1, ncomp;

	/* send initial handshake */
	pb = p9pbuf_makeout();
	p9pbuf_put_1(pb, P9PROTO_T_VERSION);
	p9pbuf_put_2(pb, P9PROTO_NOTAG);
	p9pbuf_put_4(pb, p9p->maxreq);
	p9pbuf_put_str(pb, P9PROTO_VERSION);
	DO_IO(p9pbuf_write, pu, pb, p9p->servsock, &done, rv);

	puffs_framebuf_recycle(pb);
	DO_IO(p9pbuf_read, pu, pb, p9p->servsock, &done, rv);

	if ((type = p9pbuf_get_type(pb)) != P9PROTO_R_VERSION)
		errx(1, "server invalid response to Tversion: %d", type);
	if ((rtagid = p9pbuf_get_tag(pb)) != P9PROTO_NOTAG) {
		errx(1, "server invalid tag: %d vs. %d",
		    P9PROTO_NOTAG, rtagid);
		return NULL;
	}
	if (p9pbuf_get_4(pb, &maxreq))
		errx(1, "server invalid response: no request length");
	if (maxreq < P9P_MINREQLEN)
		errx(1, "server request length below minimum accepted: "
		    "%d vs. %d", P9P_MINREQLEN, maxreq);
	p9p->maxreq = maxreq;

	/* tell the server we don't support authentication */
	p9pbuf_recycleout(pb);
	tagid = NEXTTAG(p9p);
	p9pbuf_put_1(pb, P9PROTO_T_AUTH);
	p9pbuf_put_2(pb, tagid);
	p9pbuf_put_4(pb, P9PROTO_NOFID);
	p9pbuf_put_str(pb, username);
	p9pbuf_put_str(pb, "");
	DO_IO(p9pbuf_write, pu, pb, p9p->servsock, &done, rv);

	puffs_framebuf_recycle(pb);
	DO_IO(p9pbuf_read, pu, pb, p9p->servsock, &done, rv);

	/* assume all Rerror is "no auth" */
	if (p9pbuf_get_type(pb) != P9PROTO_R_ERROR)
		errx(1, "mount_9p supports only NO auth");
	if ((rtagid = p9pbuf_get_tag(pb)) != tagid)
		errx(1, "server invalid tag: %d vs. %d", tagid, rtagid);

	/* build attach message */
	p9pbuf_recycleout(pb);
	tagid = NEXTTAG(p9p);
	p9pbuf_put_1(pb, P9PROTO_T_ATTACH);
	p9pbuf_put_2(pb, tagid);
	p9pbuf_put_4(pb, P9P_ROOTFID);
	p9pbuf_put_4(pb, P9PROTO_NOFID);
	p9pbuf_put_str(pb, username);
	p9pbuf_put_str(pb, "");
	DO_IO(p9pbuf_write, pu, pb, p9p->servsock, &done, rv);

	puffs_framebuf_recycle(pb);
	DO_IO(p9pbuf_read, pu, pb, p9p->servsock, &done, rv);

	if ((type = p9pbuf_get_type(pb)) != P9PROTO_R_ATTACH)
		errx(1, "Rattach not received, got %d", type);
	if ((rtagid = p9pbuf_get_tag(pb)) != tagid)
		errx(1, "server invalid tag: %d vs. %d", tagid, rtagid);

	/* just walk away rootfid, you won't see me follow you back home */

#define EATSLASH(p) while (*(p) == '/') p++
	assert(*path == '/');
	p = path;
	EATSLASH(p);
	for (ncomp = 0; p && *p; ncomp++) {
		EATSLASH(p);
		if (!*p)
			break;
		p = strchr(p, '/');
	}

	if (ncomp == 0) {
		curfid = P9P_ROOTFID;
	} else {
		uint16_t walked;

		p9pbuf_recycleout(pb);
		tagid = NEXTTAG(p9p);
		curfid = NEXTFID(p9p);
		p9pbuf_put_1(pb, P9PROTO_T_WALK);
		p9pbuf_put_2(pb, tagid);
		p9pbuf_put_4(pb, P9P_ROOTFID);
		p9pbuf_put_4(pb, curfid);
		p9pbuf_put_2(pb, ncomp);

		p = path;
		while (p && *p) {
			char *p2;

			EATSLASH(p);
			if (!*p)
				break;
			if ((p2 = strchr(p, '/')) == NULL)
				p2 = strchr(p, '\0');
			p9pbuf_put_data(pb, p, p2-p);
			p = p2;
		}

		DO_IO(p9pbuf_write, pu, pb, p9p->servsock, &done, rv);

		puffs_framebuf_recycle(pb);
		DO_IO(p9pbuf_read, pu, pb, p9p->servsock, &done, rv);

		if ((type = p9pbuf_get_type(pb)) != P9PROTO_R_WALK)
			errx(1, "Rwalk not received for rnode, got %d", type);
		if ((rtagid = p9pbuf_get_tag(pb)) != tagid)
			errx(1, "server invalid tag: %d vs. %d",
			    tagid, rtagid);
		if (p9pbuf_get_2(pb, &walked) == -1)
			errx(1, "can't get number of walked qids");
		if (walked != ncomp)
			errx(1, "can't locate rootpath %s, only %d/%d "
			    "components found", path, walked, ncomp);
		
		/* curfid is alive, clunk P9P_ROOTFID */
		p9pbuf_recycleout(pb);
		tagid = NEXTTAG(p9p);
		p9pbuf_put_1(pb, P9PROTO_T_CLUNK);
		p9pbuf_put_2(pb, tagid);
		p9pbuf_put_4(pb, P9P_ROOTFID);

		DO_IO(p9pbuf_write, pu, pb, p9p->servsock, &done, rv);
		puffs_framebuf_recycle(pb);
		DO_IO(p9pbuf_read, pu, pb, p9p->servsock, &done, rv);
		/* wedontcare */
	}

	/* finally, stat the node */
	p9pbuf_recycleout(pb);
	tagid = NEXTTAG(p9p);
	p9pbuf_put_1(pb, P9PROTO_T_STAT);
	p9pbuf_put_2(pb, tagid);
	p9pbuf_put_4(pb, curfid);
	DO_IO(p9pbuf_write, pu, pb, p9p->servsock, &done, rv);

	puffs_framebuf_recycle(pb);
	DO_IO(p9pbuf_read, pu, pb, p9p->servsock, &done, rv);

	if ((type = p9pbuf_get_type(pb)) != P9PROTO_R_STAT)
		errx(1, "Rstat not received, got %d", type);
	if ((rtagid = p9pbuf_get_tag(pb)) != tagid)
		errx(1, "server invalid tag: %d vs. %d", tagid, rtagid);
	if (p9pbuf_get_2(pb, &dummy))
		errx(1, "couldn't get stat len parameter");
	if (proto_getstat(pb, &rootva, NULL, NULL))
		errx(1, "could not parse root attributes");
	puffs_framebuf_destroy(pb);

	rootva.va_nlink = 0156; /* guess, will be fixed with first readdir */
	pn = newp9pnode_va(pu, &rootva, curfid);

	if (ioctl(p9p->servsock, FIONBIO, &x) == -1)
		err(1, "cannot set socket in nonblocking mode");

	return pn;
}

int
puffs9p_fs_unmount(struct puffs_usermount *pu, int flags)
{
	struct puffs9p *p9p = puffs_getspecific(pu);

	close(p9p->servsock);
	return 0;
}
