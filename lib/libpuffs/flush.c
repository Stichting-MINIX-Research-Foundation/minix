/*	$NetBSD: flush.c,v 1.16 2008/08/12 19:44:39 pooka Exp $	*/

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
#if !defined(lint)
__RCSID("$NetBSD: flush.c,v 1.16 2008/08/12 19:44:39 pooka Exp $");
#endif /* !lint */

/*
 * Flushing / invalidation routines
 */

#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <puffs.h>
#include <stdio.h>
#include <unistd.h>

#include "puffs_priv.h"

#if 0
int
puffs_inval_namecache_node(struct puffs_usermount *pu, puffs_cookie_t cookie,
	const char *name)
{

	return EOPNOTSUPP;
}
#endif

static int
doflush(struct puffs_usermount *pu, puffs_cookie_t cookie, int op,
	off_t start, off_t end)
{
	struct puffs_framebuf *pb;
	struct puffs_flush *pf;
	size_t winlen;
	int rv;

	pb = puffs_framebuf_make();
	if (pb == NULL)
		return ENOMEM;

	winlen = sizeof(struct puffs_flush);
	if ((rv = puffs_framebuf_getwindow(pb, 0, (void *)&pf, &winlen)) == -1)
		goto out;
	assert(winlen == sizeof(struct puffs_flush));

	pf->pf_req.preq_buflen = sizeof(struct puffs_flush);
	pf->pf_req.preq_opclass = PUFFSOP_FLUSH;
	pf->pf_req.preq_id = puffs__nextreq(pu);

	pf->pf_op = op;
	pf->pf_cookie = cookie;
	pf->pf_start = start;
	pf->pf_end = end;

	rv = puffs_framev_enqueue_cc(puffs_cc_getcc(pu),
	    puffs_getselectable(pu), pb, 0);

 out:
	puffs_framebuf_destroy(pb);
	return rv;
}

int
puffs_inval_namecache_dir(struct puffs_usermount *pu, puffs_cookie_t cookie)
{

	return doflush(pu, cookie, PUFFS_INVAL_NAMECACHE_DIR, 0, 0);
}

int
puffs_inval_namecache_all(struct puffs_usermount *pu)
{

	return doflush(pu, NULL, PUFFS_INVAL_NAMECACHE_ALL, 0, 0);
}

int
puffs_inval_pagecache_node(struct puffs_usermount *pu, puffs_cookie_t cookie)
{

	return doflush(pu, cookie, PUFFS_INVAL_PAGECACHE_NODE_RANGE, 0, 0);
}

int
puffs_inval_pagecache_node_range(struct puffs_usermount *pu,
	puffs_cookie_t cookie, off_t start, off_t end)
{

	return doflush(pu, cookie, PUFFS_INVAL_PAGECACHE_NODE_RANGE, start,end);
}

int
puffs_flush_pagecache_node(struct puffs_usermount *pu, puffs_cookie_t cookie)
{

	return doflush(pu, cookie, PUFFS_FLUSH_PAGECACHE_NODE_RANGE, 0, 0);
}

int
puffs_flush_pagecache_node_range(struct puffs_usermount *pu,
	puffs_cookie_t cookie, off_t start, off_t end)
{

	return doflush(pu, cookie, PUFFS_FLUSH_PAGECACHE_NODE_RANGE, start,end);
}
