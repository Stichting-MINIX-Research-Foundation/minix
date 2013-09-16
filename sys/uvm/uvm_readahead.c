/*	$NetBSD: uvm_readahead.c,v 1.8 2011/06/12 03:36:04 rmind Exp $	*/

/*-
 * Copyright (c)2003, 2005, 2009 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * uvm_object read-ahead
 *
 * TODO:
 *	- tune.
 *	- handle multiple streams.
 *	- find a better way to deal with PGO_LOCKED pager requests.
 *	  (currently just ignored)
 *	- consider the amount of memory in the system.
 *	- consider the speed of the underlying device.
 *	- consider filesystem block size / block layout.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uvm_readahead.c,v 1.8 2011/06/12 03:36:04 rmind Exp $");

#include <sys/param.h>
#include <sys/pool.h>

#include <uvm/uvm.h>
#include <uvm/uvm_readahead.h>

#if defined(READAHEAD_DEBUG)
#define	DPRINTF(a)	printf a
#else /* defined(READAHEAD_DEBUG) */
#define	DPRINTF(a)	/* nothing */
#endif /* defined(READAHEAD_DEBUG) */

/*
 * uvm_ractx: read-ahead context.
 */

struct uvm_ractx {
	int ra_flags;
#define	RA_VALID	1
	off_t ra_winstart;	/* window start offset */
	size_t ra_winsize;	/* window size */
	off_t ra_next;		/* next offset to read-ahead */
};

#if defined(sun2) || defined(sun3)
/* XXX: on sun2 and sun3 MAXPHYS is 0xe000 */
#undef MAXPHYS	
#define MAXPHYS		0x8000	/* XXX */
#endif

#define	RA_WINSIZE_INIT	MAXPHYS			/* initial window size */
#define	RA_WINSIZE_MAX	(MAXPHYS * 8)		/* max window size */
#define	RA_WINSIZE_SEQENTIAL	RA_WINSIZE_MAX	/* fixed window size used for
						   SEQUENTIAL hint */
#define	RA_MINSIZE	(MAXPHYS * 2)		/* min size to start i/o */
#define	RA_IOCHUNK	MAXPHYS			/* read-ahead i/o chunk size */

static off_t ra_startio(struct uvm_object *, off_t, size_t);
static struct uvm_ractx *ra_allocctx(void);
static void ra_freectx(struct uvm_ractx *);

static struct pool_cache ractx_cache;

/*
 * uvm_ra_init: initialize readahead module.
 */

void
uvm_ra_init(void)
{

	pool_cache_bootstrap(&ractx_cache, sizeof(struct uvm_ractx), 0, 0, 0,
	    "ractx", NULL, IPL_NONE, NULL, NULL, NULL);
}

static struct uvm_ractx *
ra_allocctx(void)
{

	return pool_cache_get(&ractx_cache, PR_NOWAIT);
}

static void
ra_freectx(struct uvm_ractx *ra)
{

	pool_cache_put(&ractx_cache, ra);
}

/*
 * ra_startio: start i/o for read-ahead.
 *
 * => start i/o for each RA_IOCHUNK sized chunk.
 * => return offset to which we started i/o.
 */

static off_t
ra_startio(struct uvm_object *uobj, off_t off, size_t sz)
{
	const off_t endoff = off + sz;

	DPRINTF(("%s: uobj=%p, off=%" PRIu64 ", endoff=%" PRIu64 "\n",
	    __func__, uobj, off, endoff));
	off = trunc_page(off);
	while (off < endoff) {
		const size_t chunksize = RA_IOCHUNK;
		int error;
		size_t donebytes;
		int npages;
		int orignpages;
		size_t bytelen;

		KASSERT((chunksize & (chunksize - 1)) == 0);
		KASSERT((off & PAGE_MASK) == 0);
		bytelen = ((off + chunksize) & -(off_t)chunksize) - off;
		KASSERT((bytelen & PAGE_MASK) == 0);
		npages = orignpages = bytelen >> PAGE_SHIFT;
		KASSERT(npages != 0);

		/*
		 * use UVM_ADV_RANDOM to avoid recursion.
		 */

		mutex_enter(uobj->vmobjlock);
		error = (*uobj->pgops->pgo_get)(uobj, off, NULL,
		    &npages, 0, VM_PROT_READ, UVM_ADV_RANDOM, 0);
		DPRINTF(("%s:  off=%" PRIu64 ", bytelen=%zu -> %d\n",
		    __func__, off, bytelen, error));
		if (error != 0 && error != EBUSY) {
			if (error != EINVAL) { /* maybe past EOF */
				DPRINTF(("%s: error=%d\n", __func__, error));
			}
			break;
		}
		KASSERT(orignpages == npages);
		donebytes = orignpages << PAGE_SHIFT;
		off += donebytes;
	}

	return off;
}

/* ------------------------------------------------------------ */

/*
 * uvm_ra_allocctx: allocate a context.
 */

struct uvm_ractx *
uvm_ra_allocctx(void)
{
	struct uvm_ractx *ra;

	ra = ra_allocctx();
	if (ra != NULL) {
		ra->ra_flags = 0;
	}

	return ra;
}

/*
 * uvm_ra_freectx: free a context.
 */

void
uvm_ra_freectx(struct uvm_ractx *ra)
{

	KASSERT(ra != NULL);
	ra_freectx(ra);
}

/*
 * uvm_ra_request: update a read-ahead context and start i/o if appropriate.
 *
 * => called when [reqoff, reqoff+reqsize) is requested.
 * => object must be locked by caller, will return locked.
 */

void
uvm_ra_request(struct uvm_ractx *ra, int advice, struct uvm_object *uobj,
    off_t reqoff, size_t reqsize)
{

	KASSERT(mutex_owned(uobj->vmobjlock));

	if (ra == NULL || advice == UVM_ADV_RANDOM) {
		return;
	}

	if (advice == UVM_ADV_SEQUENTIAL) {

		/*
		 * always do read-ahead with a large window.
		 */

		if ((ra->ra_flags & RA_VALID) == 0) {
			ra->ra_winstart = ra->ra_next = 0;
			ra->ra_flags |= RA_VALID;
		}
		if (reqoff < ra->ra_winstart) {
			ra->ra_next = reqoff;
		}
		ra->ra_winsize = RA_WINSIZE_SEQENTIAL;
		goto do_readahead;
	}

	/*
	 * a request with UVM_ADV_NORMAL hint.  (ie. no hint)
	 *
	 * we keep a sliding window in order to determine:
	 *	- if the previous read-ahead was successful or not.
	 *	- how many bytes to read-ahead.
	 */

	/*
	 * if it's the first request for this context,
	 * initialize context and return.
	 */

	if ((ra->ra_flags & RA_VALID) == 0) {
initialize:
		ra->ra_winstart = ra->ra_next = reqoff + reqsize;
		ra->ra_winsize = RA_WINSIZE_INIT;
		ra->ra_flags |= RA_VALID;
		goto done;
	}

	/*
	 * if it isn't in our window,
	 * initialize context and return.
	 * (read-ahead miss)
	 */

	if (reqoff < ra->ra_winstart ||
	    ra->ra_winstart + ra->ra_winsize < reqoff) {

		/*
		 * ... unless we seem to be reading the same chunk repeatedly.
		 *
		 * XXX should have some margin?
		 */

		if (reqoff + reqsize == ra->ra_winstart) {
			DPRINTF(("%s: %p: same block: off=%" PRIu64
			    ", size=%zd, winstart=%" PRIu64 "\n",
			    __func__, ra, reqoff, reqsize, ra->ra_winstart));
			goto done;
		}
		goto initialize;
	}

	/*
	 * it's in our window. (read-ahead hit)
	 *	- start read-ahead i/o if appropriate.
	 *	- advance and enlarge window.
	 */

do_readahead:

	/*
	 * don't bother to read-ahead behind current request.
	 */

	if (reqoff > ra->ra_next) {
		ra->ra_next = reqoff;
	}

	/*
	 * try to make [reqoff, reqoff+ra_winsize) in-core.
	 * note that [reqoff, ra_next) is considered already done.
	 */

	if (reqoff + ra->ra_winsize > ra->ra_next) {
		off_t raoff = MAX(reqoff, ra->ra_next);
		size_t rasize = reqoff + ra->ra_winsize - ra->ra_next;

#if defined(DIAGNOSTIC)
		if (rasize > RA_WINSIZE_MAX) {
			printf("%s: corrupted context", __func__);
			rasize = RA_WINSIZE_MAX;
		}
#endif /* defined(DIAGNOSTIC) */

		/*
		 * issue read-ahead only if we can start big enough i/o.
		 * otherwise we end up with a stream of small i/o.
		 */

		if (rasize >= RA_MINSIZE) {
			off_t next;

			mutex_exit(uobj->vmobjlock);
			next = ra_startio(uobj, raoff, rasize);
			mutex_enter(uobj->vmobjlock);
			ra->ra_next = next;
		}
	}

	/*
	 * update window.
	 *
	 * enlarge window by reqsize, so that it grows in a predictable manner
	 * regardless of the size of each read(2).
	 */

	ra->ra_winstart = reqoff + reqsize;
	ra->ra_winsize = MIN(RA_WINSIZE_MAX, ra->ra_winsize + reqsize);

done:;
}

int
uvm_readahead(struct uvm_object *uobj, off_t off, off_t size)
{

	/*
	 * don't allow too much read-ahead.
	 */
	if (size > RA_WINSIZE_MAX) {
		size = RA_WINSIZE_MAX;
	}
	ra_startio(uobj, off, size);
	return 0;
}
