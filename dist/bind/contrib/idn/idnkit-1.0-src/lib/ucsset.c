#ifndef lint
static char *rcsid = "$Id: ucsset.c,v 1.1.1.1 2003-06-04 00:26:15 marka Exp $";
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
#include <stdio.h>
#include <string.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/ucsset.h>

#define UCS_MAX		0x80000000UL

#define INIT_SIZE	50

/*
 * Code point range.
 *
 * The set of code points is represented by an array of code point ranges.
 * In the building phase, specified ranges by 'idn_ucsset_add' or
 * 'idn_ucsset_addrange' are simply appended to the array.
 * And 'idn_ucsset_fix' sorts the array by the code point value, and also
 * merges any intersecting ranges.  Since the array is sorted, a binary
 * search can be used for looking up.
 */
typedef struct {
	unsigned long from;
	unsigned long to;
} range_t;

/*
 * Code point segment.
 *
 * To speed up searching further, the entire region of UCS-4 code points
 * (U+0000 - U+7FFFFFFF) are divided into segments. For each segment,
 * the first and last element of the range array corresponding to the
 * segment are computed by 'idn_ucsset_fix'.  This narrows down the
 * (initial) search range.
 */
typedef struct {
	int range_start;	/* index of ucsset.ranges */
	int range_end;		/* ditto */
} segment_t;

/*
 * Code point to segment index conversion.
 *
 * Below is the function that maps a code point to the corresponding segment.
 * The mapping is non-uniform, so that BMP, the following 16 planes that
 * comprise Unicode code points together with BMP, and other planes
 * have different granularity.
 */
#define SEG_THLD1	0x10000		/* BMP */
#define SEG_THLD2	0x110000	/* Unicode (BMP+16planes) */
#define SEG_SFT1	10		/* BMP: 1K code points/segment */
#define SEG_SFT2	14		/* following 16 planes: 16K cp/seg */
#define SEG_SFT3	24		/* rest: 16M cp/seg */
#define SEG_OFF1	(SEG_THLD1 >> SEG_SFT1)
#define SEG_OFF2	(((SEG_THLD2 - SEG_THLD1) >> SEG_SFT2) + SEG_OFF1)
#define SEG_INDEX(v) \
	(((v) < SEG_THLD1) ? ((v) >> SEG_SFT1) : \
	 ((v) < SEG_THLD2) ? ((((v) - SEG_THLD1) >> SEG_SFT2) + SEG_OFF1) : \
	 ((((v) - SEG_THLD2) >> SEG_SFT3) + SEG_OFF2))
#define SEG_LEN	(SEG_INDEX(UCS_MAX - 1) + 1)

/*
 * Representation of set of UCS code points.
 */
typedef struct idn_ucsset {
	segment_t segments[SEG_LEN];
	int fixed;
	int size;			/* allocated size of 'ranges' */
	int nranges;			/* num of ranges */
	range_t *ranges;
	int refcnt;			/* reference count */
} ucsset;

static idn_result_t	addrange(idn_ucsset_t ctx, unsigned long from,
				 unsigned long to, char *func_name);
static int		comp_range(const void *v1, const void *v2);

idn_result_t
idn_ucsset_create(idn_ucsset_t *ctx) {
	idn_ucsset_t bm;

	assert(ctx != NULL);

	TRACE(("idn_ucsset_create()\n"));

	if ((bm = malloc(sizeof(ucsset))) == NULL) {
		WARNING(("idn_ucsset_create: malloc failed\n"));
		return idn_nomemory;
	}
	bm->size = bm->nranges = 0;
	bm->ranges = NULL;
	bm->fixed = 0;
	bm->refcnt = 1;
	*ctx = bm;
	return (idn_success);
}

void
idn_ucsset_destroy(idn_ucsset_t ctx) {
	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsset_destroy()\n"));

	if (--ctx->refcnt == 0) {
		if (ctx->ranges != NULL)
			free(ctx->ranges);
		free(ctx);
	}
}

void
idn_ucsset_incrref(idn_ucsset_t ctx) {
	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsset_incrref()\n"));

	ctx->refcnt++;
}

idn_result_t
idn_ucsset_add(idn_ucsset_t ctx, unsigned long v) {
	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsset_add(v=U+%lX)\n", v));

	return (addrange(ctx, v, v, "idn_ucsset_add"));
}

idn_result_t
idn_ucsset_addrange(idn_ucsset_t ctx, unsigned long from,
			 unsigned long to)
{
	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsset_addrange(from=U+%lX, to=U+%lX)\n",
	       from, to));

	return (addrange(ctx, from, to, "idn_ucsset_addrange"));
}

void
idn_ucsset_fix(idn_ucsset_t ctx) {
	int nranges;
	range_t *ranges;
	segment_t *segments;
	int i, j;

	assert(ctx != NULL && ctx->refcnt > 0);

	TRACE(("idn_ucsset_fix()\n"));

	nranges = ctx->nranges;
	ranges = ctx->ranges;
	segments = ctx->segments;

	if (ctx->fixed)
		return;

	ctx->fixed = 1;

	/* Initialize segment array */
	for (i = 0; i < SEG_LEN; i++) {
		segments[i].range_start = -1;
		segments[i].range_end = -1;
	}

	/* If the set is empty, there's nothing to be done. */
	if (nranges == 0)
		return;

	/* Sort ranges. */
	qsort(ranges, nranges, sizeof(range_t), comp_range);

	/* Merge overlapped/continuous ranges. */
	for (i = 0, j = 1; j < nranges; j++) {
		if (ranges[i].to + 1 >= ranges[j].from) {
			/* can be merged */
			if (ranges[i].to < ranges[j].to) {
				ranges[i].to = ranges[j].to;
			}
		} else {
			i++;
			if (i < j)
				ranges[i] = ranges[j];
		}
	}
	/* 'i' points the last range in the array. */
	ctx->nranges = nranges = ++i;

	/* Create segment array. */
	for (i = 0; i < nranges; i++) {
		int fidx = SEG_INDEX(ranges[i].from);
		int tidx = SEG_INDEX(ranges[i].to);

		for (j = fidx; j <= tidx; j++) {
			if (segments[j].range_start < 0)
				segments[j].range_start = i;
			segments[j].range_end = i;
		}
	}

#if 0
	/*
	 * Does the standard guarantee realloc() always succeeds
	 * when shrinking?
	 */
	/* Shrink malloc'ed space if possible. */
	ctx->ranges = realloc(ctx->ranges, ctx->nranges * sizeof(range_t));
#endif
}

idn_result_t
idn_ucsset_lookup(idn_ucsset_t ctx, unsigned long v, int *found) {
	int idx;
	segment_t *segments;

	assert(ctx != NULL && ctx->refcnt > 0 && found != NULL);

	TRACE(("idn_ucsset_lookup(v=U+%lX)\n", v));

	/* Make sure it is fixed. */
	if (!ctx->fixed) {
		WARNING(("idn_ucsset_lookup: not fixed yet\n"));
		return (idn_failure);
	}

	/* Check the given code point. */
	if (v >= UCS_MAX)
		return (idn_invalid_codepoint);

	/* Get the segment 'v' belongs to. */
	segments = ctx->segments;
	idx = SEG_INDEX(v);

	/* Do binary search. */
	*found = 0;
	if (segments[idx].range_start >= 0) {
		int lo = segments[idx].range_start;
		int hi = segments[idx].range_end;
		range_t *ranges = ctx->ranges;

		while (lo <= hi) {
			int mid = (lo + hi) / 2;
			if (v < ranges[mid].from) {
				hi = mid - 1;
			} else if (v > ranges[mid].to) {
				lo = mid + 1;
			} else {
				*found = 1;
				break;
			}
		}
	}
	return (idn_success);
}

static idn_result_t
addrange(idn_ucsset_t ctx, unsigned long from, unsigned long to,
	 char *func_name)
{
	range_t *newbuf;

	/* Check the given code points. */
	if (from > UCS_MAX) {
		WARNING(("%s: code point out of range (U+%lX)\n",
			 func_name, from));
		return (idn_invalid_codepoint);
	} else if (to > UCS_MAX) {
		WARNING(("%s: code point out of range (U+%lX)\n",
			 func_name, to));
		return (idn_invalid_codepoint);
	} else if (from > to) {
		WARNING(("%s: invalid range spec (U+%lX-U+%lX)\n",
			 func_name, from, to));
		return (idn_invalid_codepoint);
	}

	/* Make sure it is not fixed yet. */
	if (ctx->fixed) {
		WARNING(("%s: attempt to add to already fixed object\n",
			 func_name));
		return (idn_failure);
	}

	/* Append the specified range to the 'ranges' array. */
	if (ctx->nranges >= ctx->size) {
		/* Make it bigger. */
		if (ctx->size == 0)
			ctx->size = INIT_SIZE;
		else
			ctx->size *= 2;
		newbuf = realloc(ctx->ranges, ctx->size * sizeof(range_t));
		if (newbuf == NULL)
			return (idn_nomemory);
		ctx->ranges = newbuf;
	}
	ctx->ranges[ctx->nranges].from = from;
	ctx->ranges[ctx->nranges].to = to;
	ctx->nranges++;

	return (idn_success);
}

static int
comp_range(const void *v1, const void *v2) {
	/*
	 * Range comparation function suitable for qsort().
	 */
	const range_t *r1 = v1;
	const range_t *r2 = v2;

	if (r1->from < r2->from)
		return (-1);
	else if (r1->from > r2->from)
		return (1);
	else
		return (0);
}
