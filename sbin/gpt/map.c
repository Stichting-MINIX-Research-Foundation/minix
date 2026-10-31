/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD: src/sbin/gpt/map.c,v 1.6 2005/08/31 01:47:19 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: map.c,v 1.15 2020/05/24 14:42:44 jmcneill Exp $");
#endif

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static map_t
map_create(off_t start, off_t size, int type)
{
	map_t m;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		return NULL;
	m->map_start = start;
	m->map_size = size;
	m->map_next = m->map_prev = NULL;
	m->map_type = type;
	m->map_index = 0;
	m->map_data = NULL;
	m->map_alloc = 0;
	return m;
}

static void
map_destroy(map_t m)
{
	if (m == NULL)
		return;
	if (m->map_alloc)
		free(m->map_data);
	free(m);
}

static const char *maptypes[] = {
	"unused",
	"mbr",
	"mbr partition",
	"primary gpt header",
	"secondary gpt header",
	"primary gpt table",
	"secondary gpt table",
	"gpt partition",
	"protective mbr",
};

static const char *
map_type(int t)
{
	if ((size_t)t >= __arraycount(maptypes))
		return "*unknown*";
	return maptypes[t];
}

map_t
map_add(gpt_t gpt, off_t start, off_t size, int type, void *data, int alloc)
{
	map_t m, n, p;

#ifdef DEBUG
	printf("add: %s %#jx %#jx\n", map_type(type), (uintmax_t)start,
	    (uintmax_t)size);
	for (n = gpt->mediamap; n; n = n->map_next)
		printf("have: %s %#jx %#jx\n", map_type(n->map_type),
		    (uintmax_t)n->map_start, (uintmax_t)n->map_size);
#endif

	n = gpt->mediamap;
	while (n != NULL && n->map_start + n->map_size <= start)
		n = n->map_next;
	if (n == NULL) {
		if (!(gpt->flags & GPT_QUIET))
			gpt_warnx(gpt, "Can't find map");
		return NULL;
	}

	if (n->map_start + n->map_size < start + size) {
		if (!(gpt->flags & GPT_QUIET))
			gpt_warnx(gpt, "map entry doesn't fit media: "
			    "new start + new size < start + size\n"
			    "(%jx + %jx < %jx + %jx)",
			    n->map_start, n->map_size, start, size);
		return NULL;
	}

	if (n->map_start == start && n->map_size == size) {
		if (n->map_type != MAP_TYPE_UNUSED) {
			if (n->map_type != MAP_TYPE_MBR_PART ||
			    type != MAP_TYPE_GPT_PART) {
				if (!(gpt->flags & GPT_QUIET))
					gpt_warnx(gpt,
					    "partition(%ju,%ju) mirrored",
					    (uintmax_t)start, (uintmax_t)size);
			}
		}
		n->map_type = type;
		n->map_data = data;
		n->map_alloc = alloc;
		return n;
	}

	if (n->map_type != MAP_TYPE_UNUSED) {
		if (n->map_type != MAP_TYPE_MBR_PART ||
		    type != MAP_TYPE_GPT_PART) {
			gpt_warnx(gpt, "bogus map current=%s new=%s",
			    map_type(n->map_type), map_type(type));
			return NULL;
		}
		n->map_type = MAP_TYPE_UNUSED;
	}

	m = map_create(start, size, type);
	if (m == NULL)
		goto oomem;

	m->map_data = data;
	m->map_alloc = alloc;

	if (start == n->map_start) {
		m->map_prev = n->map_prev;
		m->map_next = n;
		if (m->map_prev != NULL)
			m->map_prev->map_next = m;
		else
			gpt->mediamap = m;
		n->map_prev = m;
		n->map_start += size;
		n->map_size -= size;
	} else if (start + size == n->map_start + n->map_size) {
		p = n;
		m->map_next = p->map_next;
		m->map_prev = p;
		if (m->map_next != NULL)
			m->map_next->map_prev = m;
		p->map_next = m;
		p->map_size -= size;
	} else {
		p = map_create(n->map_start, start - n->map_start, n->map_type);
		if (p == NULL)
			goto oomem;
		n->map_start += p->map_size + m->map_size;
		n->map_size -= (p->map_size + m->map_size);
		p->map_prev = n->map_prev;
		m->map_prev = p;
		n->map_prev = m;
		m->map_next = n;
		p->map_next = m;
		if (p->map_prev != NULL)
			p->map_prev->map_next = p;
		else
			gpt->mediamap = p;
	}

	return m;
oomem:
	map_destroy(m);
	gpt_warn(gpt, "Can't create map");
	return NULL;
}

map_t
map_alloc(gpt_t gpt, off_t start, off_t size, off_t alignment)
{
	off_t delta;
	map_t m;

	if (alignment > 0) {
		if ((start % alignment) != 0)
			start = (start + alignment) / alignment * alignment;
		if ((size % alignment) != 0)
			size = (size + alignment) / alignment * alignment;
	}

	for (m = gpt->mediamap; m != NULL; m = m->map_next) {
		if (m->map_type != MAP_TYPE_UNUSED || m->map_start < 2)
			continue;
		if (start != 0 && m->map_start > start)
			return NULL;

		if (start != 0)
			delta = start - m->map_start;
		else if (alignment > 0 && m->map_start % alignment != 0)
			delta = (m->map_start + alignment) /
			        alignment * alignment - m->map_start;
		else
			delta = 0;

                if (size == 0 || m->map_size - delta >= size) {
			if (m->map_size - delta < alignment)
				continue;
			if (size == 0) {
				if (alignment > 0 &&
				    (m->map_size - delta) % alignment != 0)
					size = (m->map_size - delta) /
					    alignment * alignment;
				else
					size = m->map_size - delta;
			}
			return map_add(gpt, m->map_start + delta, size,
			    MAP_TYPE_GPT_PART, NULL, 0);
		}
	}

	return NULL;
}

off_t
map_resize(gpt_t gpt, map_t m, off_t size, off_t alignment)
{
	map_t n, o;
	off_t alignsize, prevsize;

	n = m->map_next;

	if (size < 0 || alignment < 0) {
		gpt_warnx(gpt, "negative size or alignment");
		return -1;
	}
	/* Size == 0 means delete, if the next map is unused */
	if (size == 0) { 
		if (n == NULL) {
			// XXX: we could just turn the map to UNUSED!
			gpt_warnx(gpt, "Can't delete, next map is not found");
			return -1;
		}
		if (n->map_type != MAP_TYPE_UNUSED) {
			gpt_warnx(gpt, "Can't delete, next map is in use");
			return -1;
		}
		if (alignment == 0) {
			size = m->map_size + n->map_size;
			m->map_size = size;
			m->map_next = n->map_next;
			if (n->map_next != NULL)
				n->map_next->map_prev = m;
			map_destroy(n);
			return size;
		} else { /* alignment > 0 */
			prevsize = m->map_size;
			size = ((m->map_size + n->map_size) / alignment)
			    * alignment;
			if (size == prevsize) {
				m->map_size = size;
				return size;
			} else if (size < prevsize) {
				gpt_warnx(gpt, "Can't coalesce %ju <= %ju",
				    (uintmax_t)prevsize, (uintmax_t)size);
				return -1;
			}
			m->map_size = size;
			n->map_start += size - prevsize;
			n->map_size -= size - prevsize;
			if (n->map_size == 0) {
				m->map_next = n->map_next;
				if (n->map_next != NULL)
					n->map_next->map_prev = m;
				map_destroy(n);
			}
			return size;
		}
	}
			
	alignsize = size;
	if (alignment % size != 0)
		alignsize = (size + alignment) / alignment * alignment;

	if (alignsize < m->map_size) {		/* shrinking */
		prevsize = m->map_size;
		m->map_size = alignsize;
		if (n == NULL || n->map_type != MAP_TYPE_UNUSED) {
			o = map_create(m->map_start + alignsize,
				  prevsize - alignsize, MAP_TYPE_UNUSED);
			if (o == NULL) {
				gpt_warn(gpt, "Can't create map");
				return -1;
			}
			m->map_next = o;
			o->map_prev = m;
			o->map_next = n;
			if (n != NULL)
				n->map_prev = o;
			return alignsize;
		} else {
			n->map_start -= alignsize;
			n->map_size += alignsize;
			return alignsize;
		}
	} else if (alignsize > m->map_size) {		/* expanding */
		if (n == NULL) {
			gpt_warnx(gpt, "Can't expand map, no space after it");
			return -1;
		}
		if (n->map_type != MAP_TYPE_UNUSED) {
			gpt_warnx(gpt,
			    "Can't expand map, next map after it in use");
			return -1;
		}
		if (n->map_size < alignsize - m->map_size) {
			gpt_warnx(gpt,
			    "Can't expand map, not enough space in the"
			    " next map after it");
			return -1;
		}
		n->map_size -= alignsize - m->map_size;
		n->map_start += alignsize - m->map_size;
		if (n->map_size == 0) {
			m->map_next = n->map_next;
			if (n->map_next != NULL)
				n->map_next->map_prev = m;
			map_destroy(n);
		}
		m->map_size = alignsize;
		return alignsize;
	} else						/* correct size */
		return alignsize;
}

map_t
map_find(gpt_t gpt, int type)
{
	map_t m;

	m = gpt->mediamap;
	while (m != NULL && m->map_type != type)
		m = m->map_next;
	return m;
}

map_t
map_first(gpt_t gpt)
{
	return gpt->mediamap;
}

map_t
map_last(gpt_t gpt)
{
	map_t m;

	m = gpt->mediamap;
	while (m != NULL && m->map_next != NULL)
		m = m->map_next;
	return m;
}

off_t
map_free(gpt_t gpt, off_t start, off_t size)
{
	map_t m;

	m = gpt->mediamap;

	while (m != NULL && m->map_start + m->map_size <= start)
		m = m->map_next;
	if (m == NULL || m->map_type != MAP_TYPE_UNUSED)
		return 0LL;
	if (size)
		return (m->map_start + m->map_size >= start + size) ? 1 : 0;
	return m->map_size - (start - m->map_start);
}

int
map_init(gpt_t gpt, off_t size)
{
	char buf[32];

	gpt->mediamap = map_create(0LL, size, MAP_TYPE_UNUSED);
	if (gpt->mediamap == NULL) {
		gpt_warn(gpt, "Can't create map");
		return -1;
	}
	gpt->lbawidth = snprintf(buf, sizeof(buf), "%ju", (uintmax_t)size);
	if (gpt->lbawidth < 5)
		gpt->lbawidth = 5;
	return 0;
}
