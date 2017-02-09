/* $NetBSD: dtv_scatter.c,v 1.2 2014/08/09 13:34:10 jmcneill Exp $ */

/*
 * Copyright (c) 2008 Patrick Mahoney <pat@polycrystal.org>
 * All rights reserved.
 *
 * This code was written by Patrick Mahoney (pat@polycrystal.org) as
 * part of Google Summer of Code 2008.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dtv_scatter.c,v 1.2 2014/08/09 13:34:10 jmcneill Exp $");

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/kmem.h>
#include <sys/pool.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/condvar.h>
#include <sys/queue.h>

#include <dev/dtv/dtvvar.h>

void
dtv_scatter_buf_init(struct dtv_scatter_buf *sb)
{
	sb->sb_pool = pool_cache_init(PAGE_SIZE, 0, 0, 0,
				      "dtvscatter", NULL, IPL_SCHED,
				      NULL, NULL, NULL);
	sb->sb_size = 0;
	sb->sb_npages = 0;
	sb->sb_page_ary = NULL;
}

void
dtv_scatter_buf_destroy(struct dtv_scatter_buf *sb)
{
	/* Do we need to return everything to the pool first? */
	dtv_scatter_buf_set_size(sb, 0);
	pool_cache_destroy(sb->sb_pool);
	sb->sb_pool = 0;
	sb->sb_npages = 0;
	sb->sb_page_ary = NULL;
}

/* Increase or decrease the size of the buffer */
int
dtv_scatter_buf_set_size(struct dtv_scatter_buf *sb, size_t sz)
{
	unsigned int i;
	size_t npages, minpages, oldnpages;
	uint8_t **old_ary;

	npages = (sz >> PAGE_SHIFT) + ((sz & PAGE_MASK) > 0);
	
	if (sb->sb_npages == npages) {
		return 0;
	}

	oldnpages = sb->sb_npages;
	old_ary = sb->sb_page_ary;

	sb->sb_npages = npages;
	if (npages > 0) {
		sb->sb_page_ary =
		    kmem_alloc(sizeof(uint8_t *) * npages, KM_SLEEP);
		if (sb->sb_page_ary == NULL) {
			sb->sb_npages = oldnpages;
			sb->sb_page_ary = old_ary;
			return ENOMEM;
		}
	} else {
		sb->sb_page_ary = NULL;
	}

	minpages = min(npages, oldnpages);
	/* copy any pages that will be reused */
	for (i = 0; i < minpages; ++i)
		sb->sb_page_ary[i] = old_ary[i];
	/* allocate any new pages */
	for (; i < npages; ++i) {
		sb->sb_page_ary[i] = pool_cache_get(sb->sb_pool, 0);
		/* TODO: does pool_cache_get return NULL on
		 * ENOMEM?  If so, we need to release or note
		 * the pages with did allocate
		 * successfully. */
		if (sb->sb_page_ary[i] == NULL) {
			return ENOMEM;
		}
	}
	/* return any pages no longer needed */
	for (; i < oldnpages; ++i)
		pool_cache_put(sb->sb_pool, old_ary[i]);

	if (old_ary != NULL)
		kmem_free(old_ary, sizeof(uint8_t *) * oldnpages);

	sb->sb_size = sb->sb_npages << PAGE_SHIFT;
	
	return 0;
}


paddr_t
dtv_scatter_buf_map(struct dtv_scatter_buf *sb, off_t off)
{
	size_t pg;
	paddr_t pa;
	
	pg = off >> PAGE_SHIFT;

	if (pg >= sb->sb_npages)
		return -1;
	else if (!pmap_extract(pmap_kernel(), (vaddr_t)sb->sb_page_ary[pg], &pa))
		return -1;

	return atop(pa);
}

/* Initialize data for an io operation on a scatter buffer. Returns
 * true if the transfer is valid, or false if out of range. */
bool
dtv_scatter_io_init(struct dtv_scatter_buf *sb,
		    off_t off, size_t len,
		    struct dtv_scatter_io *sio)
{
	if ((off + len) > sb->sb_size) {
		printf("dtv: %s failed: off=%" PRId64
			 " len=%zu sb->sb_size=%zu\n",
			 __func__, off, len, sb->sb_size);
		return false;
	}

	sio->sio_buf = sb;
	sio->sio_offset = off;
	sio->sio_resid = len;

	return true;
}

/* Store the pointer and size of the next contiguous segment.  Returns
 * true if the segment is valid, or false if all has been transfered.
 * Does not check for overflow. */
bool
dtv_scatter_io_next(struct dtv_scatter_io *sio, void **p, size_t *sz)
{
	size_t pg, pgo;

	if (sio->sio_resid == 0)
		return false;
	
	pg = sio->sio_offset >> PAGE_SHIFT;
	pgo = sio->sio_offset & PAGE_MASK;

	*sz = min(PAGE_SIZE - pgo, sio->sio_resid);
	*p = sio->sio_buf->sb_page_ary[pg] + pgo;

	sio->sio_offset += *sz;
	sio->sio_resid -= *sz;

	return true;
}

/* Semi-undo of a failed segment copy.  Updates the scatter_io
 * struct to the previous values prior to a failed segment copy. */
void
dtv_scatter_io_undo(struct dtv_scatter_io *sio, size_t sz)
{
	sio->sio_offset -= sz;
	sio->sio_resid += sz;
}

/* Copy data from src into the scatter_buf as described by io. */
void
dtv_scatter_io_copyin(struct dtv_scatter_io *sio, const void *p)
{
	void *dst;
	const uint8_t *src = p;
	size_t sz;

	while (dtv_scatter_io_next(sio, &dst, &sz)) {
		memcpy(dst, src, sz);
		src += sz;
	}
}

/* --not used; commented to avoid compiler warnings--
void
dtv_scatter_io_copyout(struct dtv_scatter_io *sio, void *p)
{
	void *src;
	uint8_t *dst = p;
	size_t sz;

	while (dtv_scatter_io_next(sio, &src, &sz)) {
		memcpy(dst, src, sz);
		dst += sz;
	}
}
*/

/* Performat a series of uiomove calls on a scatter buf.  Returns
 * EFAULT if uiomove EFAULTs on the first segment.  Otherwise, returns
 * an incomplete transfer but with no error. */
int
dtv_scatter_io_uiomove(struct dtv_scatter_io *sio, struct uio *uio)
{
	void *p;
	size_t sz;
	bool first = true;
	int err;
	
	while (dtv_scatter_io_next(sio, &p, &sz)) {
		err = uiomove(p, sz, uio);
		if (err == EFAULT) {
			dtv_scatter_io_undo(sio, sz);
			if (first)
				return EFAULT;
			else
				return 0;
		}
		first = false;
	}

	return 0;
}
