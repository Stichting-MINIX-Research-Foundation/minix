#ifndef lint
static char *rcsid = "$Id: ucsmap.c,v 1.1.1.1 2003-06-04 00:26:14 marka Exp $";
#endif

/*
 * Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/log.h>
#include <idn/logmacro.h>
#include <idn/ucsmap.h>

#define INIT_SIZE		50
#define DEFAULT_BUF_SIZE	500
#define UCSMAP_HASH_SIZE	103
#define MAX_MAPLEN		0xffff

/*
 * This module implements UCS 1-to-N mapping.
 * To speed up mapping table lookup, a combination of hash and
 * binary search is used.
 */

/*
 * Mapping entry.
 * Entries are sorted by its hash index and code point.
 */
typedef struct {
	short hidx;		/* hash index */
	unsigned short len;	/* length of mapped sequence */
	unsigned long ucs;	/* code point to be mapped */
	unsigned long *map;	/* mapped sequence of code points */
} ucsmap_entry_t;

/*
 * Hash table entry.
 * Since the entries pointed by ucsmap_hash_t.entry are sorted,
 * binary search can be used.
 */
typedef struct {
	ucsmap_entry_t *entry;	/* sorted by code point */
	int n;			/* length of 'entry' */
} ucsmap_hash_t;

/*
 * UCS character buffer for storing target character sequence.
 */
typedef struct ucsmap_buf {
	struct ucsmap_buf *next;
	unsigned long buf[1];		/* actually a variable length array */
} ucsmap_buf_t;

/*
 * Mapping object.
 */
typedef struct idn_ucsmap {
	ucsmap_hash_t hash[UCSMAP_HASH_SIZE];
	ucsmap_entry_t *entries;	/* array of entries */
	size_t entry_size;		/* allocated size */
	size_t nentries;		/* # of entries in use */
	ucsmap_buf_t *mapdata;		/* list of character buffers */
	size_t mapdata_size;		/* allocated size of current buffer */
	size_t mapdata_used;		/* # of chars in use */
	int fixed;			/* already fixed? */
	int refcnt;			/* reference count */
} ucsmap_t;

static int		ucsmap_hash(unsigned long v);
static unsigned long	*save_mapped_sequence(idn_ucsmap_t ctx,
					      unsigned long *map,
					      size_t maplen);
static void		free_mapbuf(ucsmap_buf_t *buf);
static int		comp_entry(const void *v1, const void *v2);

idn_result_t
idn_ucsmap_create(idn_ucsmap_t *ctxp) {
	idn_ucsmap_t ctx;

	assert(ctxp != NULL);

	TRACE(("idn_ucsmap_create()\n"));

	if ((ctx = malloc(sizeof(*ctx))) == NULL) {
		WARNING(("idn_ucsmap_create: malloc failed\n"));
		return (idn_nomemory);
	}

	ctx->entry_size = 0;
	ctx->nentries = 0;
	ctx->entries = NULL;
	ctx->mapdata = NULL;
	ctx->mapdata_size = 0;
	ctx->mapdata_used = 0;
	ctx->fixed = 0;
	ctx->refcnt = 1;
	*ctxp = ctx;
	return (idn_success);
}

void
idn_ucsmap_destroy(idn_ucsmap_t ctx) {
	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsmap_destroy()\n"));

	if (--ctx->refcnt == 0) {
		if (ctx->entries != NULL)
			free(ctx->entries);
		if (ctx->mapdata != NULL)
			free_mapbuf(ctx->mapdata);
		free(ctx);
	}
}

void
idn_ucsmap_incrref(idn_ucsmap_t ctx) {
	assert(ctx != NULL && ctx->refcnt > 0);

	ctx->refcnt++;
}

idn_result_t
idn_ucsmap_add(idn_ucsmap_t ctx, unsigned long ucs,
	       unsigned long *map, size_t maplen)
{
	ucsmap_entry_t *e;
	ucsmap_entry_t *newbuf;

	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsmap_add(ucs=U+%lX, maplen=%u)\n", ucs, maplen));

	/* Make sure it is not fixed yet. */
	if (ctx->fixed) {
		WARNING(("idn_ucsmap_add: attempt to add to fixed map\n"));
		return (idn_failure);
	}

	if (maplen > MAX_MAPLEN) {
		WARNING(("idn_ucsmap_add: maplen too large (> %d)\n",
			 MAX_MAPLEN));
		return (idn_failure);
	}

	/* Append an entry. */
	if (ctx->nentries >= ctx->entry_size) {
		if (ctx->entry_size == 0)
			ctx->entry_size = INIT_SIZE;
		else
			ctx->entry_size *= 2;
		newbuf = realloc(ctx->entries, sizeof(*e) * ctx->entry_size);
		if (newbuf == NULL)
			return (idn_nomemory);
		ctx->entries = newbuf;
	}
	e = &ctx->entries[ctx->nentries];
	e->hidx = ucsmap_hash(ucs);
	e->len = maplen;
	e->ucs = ucs;
	if (maplen > 0) {
		/* Save mapped sequence in the buffer. */
		e->map = save_mapped_sequence(ctx, map, maplen);
		if (e->map == NULL)
			return (idn_nomemory);
	} else {
		/*
		 * Zero 'maplen' is perfectly valid meaning one-to-zero
		 * mapping.
		 */
		e->map = NULL;
	}
	ctx->nentries++;

	return (idn_success);
}

void
idn_ucsmap_fix(idn_ucsmap_t ctx) {
	ucsmap_entry_t *e;
	int last_hidx;
	int i;

	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsmap_fix()\n"));

	if (ctx->fixed)
		return;

	ctx->fixed = 1;

	/* Initialize hash. */
	for (i = 0; i < UCSMAP_HASH_SIZE; i++) {
		ctx->hash[i].entry = NULL;
		ctx->hash[i].n = 0;
	}

	if (ctx->nentries == 0)
		return;

	/* Sort entries by the hash value and code point. */
	qsort(ctx->entries, ctx->nentries, sizeof(ucsmap_entry_t), comp_entry);

	/*
	 * Now the entries are sorted by their hash value, and
	 * sorted by its code point among the ones with the same hash value.
	 */

	/* Build hash table. */
	last_hidx = -1;
	for (i = 0, e = ctx->entries; i < ctx->nentries; i++, e++) {
		if (e->hidx != last_hidx) {
			ctx->hash[e->hidx].entry = e;
			last_hidx = e->hidx;
		}
		ctx->hash[last_hidx].n++;
	}
}

idn_result_t
idn_ucsmap_map(idn_ucsmap_t ctx, unsigned long v, unsigned long *to,
	       size_t tolen, size_t *maplenp) {
	int hash;
	ucsmap_entry_t *e;
	int n;
	int hi, lo, mid;

	assert(ctx != NULL && ctx->refcnt > 0 && to != NULL &&
	       maplenp != NULL);

	TRACE(("idn_ucsmap_map(v=U+%lX)\n", v));

	if (!ctx->fixed) {
		WARNING(("idn_ucsmap_map: not fixed yet\n"));
		return (idn_failure);
	}

	/* First, look up hash table. */
	hash = ucsmap_hash(v);
	if ((n = ctx->hash[hash].n) == 0)
		goto nomap;

	/* Then do binary search. */
	e = ctx->hash[hash].entry;
	lo = 0;
	hi = n - 1;
	while (lo <= hi) {
		mid = (lo + hi) / 2;
		if (v < e[mid].ucs)
			hi = mid - 1;
		else if (v > e[mid].ucs)
			lo = mid + 1;
		else {
			/* Found. */
			if (tolen < e[mid].len)
				return (idn_buffer_overflow);
			memcpy(to, e[mid].map, sizeof(*to) * e[mid].len);
			*maplenp = e[mid].len;
			return (idn_success);
		}
	}

	/*
	 * Not found. Put the original character to 'to'
	 * just for convenience.
	 */
 nomap:
	if (tolen < 1)
		return (idn_buffer_overflow);
	*to = v;
	*maplenp = 1;
	return (idn_nomapping);
}

static int
ucsmap_hash(unsigned long v) {
	return (v % UCSMAP_HASH_SIZE);
}

static unsigned long *
save_mapped_sequence(idn_ucsmap_t ctx, unsigned long *map, size_t maplen) {
	ucsmap_buf_t *buf;
	unsigned long *p;
	size_t allocsize;

	/*
	 * If the current buffer (the first one in the ctx->mapdata list)
	 * has enough space, use it.  Otherwise, allocate a new buffer and
	 * insert it at the beginning of the list.
	 */
	if (ctx->mapdata_used + maplen > ctx->mapdata_size) {
		if (maplen > DEFAULT_BUF_SIZE)
			allocsize = maplen * 2;
		else
			allocsize = DEFAULT_BUF_SIZE;
		buf = malloc(sizeof(ucsmap_hash_t) +
			     sizeof(unsigned long) * allocsize);
		if (buf == NULL)
			return (NULL);
		buf->next = ctx->mapdata;
		ctx->mapdata = buf;
		ctx->mapdata_size = allocsize;
		ctx->mapdata_used = 0;
	}
	p = ctx->mapdata->buf + ctx->mapdata_used;
	memcpy(p, map, sizeof(unsigned long) * maplen);
	ctx->mapdata_used += maplen;
	return (p);
}

static void
free_mapbuf(ucsmap_buf_t *buf) {
	while (buf != NULL) {
		ucsmap_buf_t *next = buf->next;
		free(buf);
		buf = next;
	}
}

static int
comp_entry(const void *v1, const void *v2) {
	const ucsmap_entry_t *e1 = v1;
	const ucsmap_entry_t *e2 = v2;

	if (e1->hidx < e2->hidx)
		return (-1);
	else if (e1->hidx > e2->hidx)
		return (1);
	else if (e1->ucs < e2->ucs)
		return (-1);
	else if (e1->ucs > e2->ucs)
		return (1);
	else
		return (0);
}
