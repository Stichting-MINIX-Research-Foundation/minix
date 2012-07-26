#ifndef lint
static char *rcsid = "$Id: unormalize.c,v 1.1.1.1 2003-06-04 00:26:43 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2001,2002 Japan Network Information Center.
 * All rights reserved.
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/ucs4.h>
#include <idn/unicode.h>
#include <idn/unormalize.h>
#include <idn/debug.h>

#if !defined(HAVE_MEMMOVE) && defined(HAVE_BCOPY)
#define memmove(a,b,c)	bcopy((char *)(b),(char *)(a),(int)(c))
#endif

#define WORKBUF_SIZE		128
#define WORKBUF_SIZE_MAX	10000

typedef struct {
	idn__unicode_version_t version; /* Unicode version */
	int cur;		/* pointing now processing character */
	int last;		/* pointing just after the last character */
	int size;		/* size of UCS and CLASS array */
	unsigned long *ucs4;	/* UCS-4 characters */
	int *class;		/* and their canonical classes */
	unsigned long ucs4_buf[WORKBUF_SIZE];	/* local buffer */
	int class_buf[WORKBUF_SIZE];		/* ditto */
} workbuf_t;

static idn_result_t	normalize(idn__unicode_version_t version,
				  int do_composition, int compat,
				  const unsigned long *from,
				  unsigned long *to, size_t tolen);
static idn_result_t	decompose(workbuf_t *wb, unsigned long c, int compat);
static void		get_class(workbuf_t *wb);
static void		reorder(workbuf_t *wb);
static void		compose(workbuf_t *wb);
static idn_result_t	flush_before_cur(workbuf_t *wb,
					 unsigned long **top, size_t *tolenp);
static void		workbuf_init(workbuf_t *wb);
static void		workbuf_free(workbuf_t *wb);
static idn_result_t	workbuf_extend(workbuf_t *wb);
static idn_result_t	workbuf_append(workbuf_t *wb, unsigned long c);
static void		workbuf_shift(workbuf_t *wb, int shift);
static void		workbuf_removevoid(workbuf_t *wb);

idn_result_t
idn__unormalize_formkc(idn__unicode_version_t version,
		       const unsigned long *from, unsigned long *to,
		       size_t tolen) {
	assert(version != NULL && from != NULL && to != NULL && tolen >= 0);
	TRACE(("idn__unormalize_formkc(from=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(from, 50), tolen));
	return (normalize(version, 1, 1, from, to, tolen));
}

static idn_result_t
normalize(idn__unicode_version_t version, int do_composition, int compat,
	  const unsigned long *from, unsigned long *to, size_t tolen) {
	workbuf_t wb;
	idn_result_t r = idn_success;

	/*
	 * Initialize working buffer.
	 */
	workbuf_init(&wb);
	wb.version = version;

	while (*from != '\0') {
		unsigned long c;

		assert(wb.cur == wb.last);

		/*
		 * Get one character from 'from'.
		 */
		c = *from++;

		/*
		 * Decompose it.
		 */
		if ((r = decompose(&wb, c, compat)) != idn_success)
			goto ret;

		/*
		 * Get canonical class.
		 */
		get_class(&wb);

		/*
		 * Reorder & compose.
		 */
		for (; wb.cur < wb.last; wb.cur++) {
			if (wb.cur == 0) {
				continue;
			} else if (wb.class[wb.cur] > 0) {
				/*
				 * This is not a starter. Try reordering.
				 * Note that characters up to it are
				 * already in canonical order.
				 */
				reorder(&wb);
				continue;
			}

			/*
			 * This is a starter character, and there are
			 * some characters before it.  Those characters
			 * have been reordered properly, and
			 * ready for composition.
			 */
			if (do_composition && wb.class[0] == 0)
				compose(&wb);

			/*
			 * If CUR points to a starter character,
			 * then process of characters before CUR are
			 * already finished, because any further
			 * reordering/composition for them are blocked
			 * by the starter CUR points.
			 */
			if (wb.cur > 0 && wb.class[wb.cur] == 0) {
				/* Flush everything before CUR. */
				r = flush_before_cur(&wb, &to, &tolen);
				if (r != idn_success)
					goto ret;
			}
		}
	}

	if (r == idn_success) {
		if (do_composition && wb.cur > 0 && wb.class[0] == 0) {
			/*
			 * There is some characters left in WB.
			 * They are ordered, but not composed yet.
			 * Now CUR points just after the last character in WB,
			 * and since compose() tries to compose characters
			 * between top and CUR inclusive, we must make CUR
			 * one character back during compose().
			 */
			wb.cur--;
			compose(&wb);
			wb.cur++;
		}
		/*
		 * Call this even when WB.CUR == 0, to make TO
		 * NUL-terminated.
		 */
		r = flush_before_cur(&wb, &to, &tolen);
		if (r != idn_success)
			goto ret;
	}

	if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*to = '\0';

ret:
	workbuf_free(&wb);
	return (r);
}

static idn_result_t
decompose(workbuf_t *wb, unsigned long c, int compat) {
	idn_result_t r;
	int dec_len;

again:
	r = idn__unicode_decompose(wb->version, compat, wb->ucs4 + wb->last,
				   wb->size - wb->last, c, &dec_len);
	switch (r) {
	case idn_success:
		wb->last += dec_len;
		return (idn_success);
	case idn_notfound:
		return (workbuf_append(wb, c));
	case idn_buffer_overflow:
		if ((r = workbuf_extend(wb)) != idn_success)
			return (r);
		if (wb->size > WORKBUF_SIZE_MAX) {
			WARNING(("idn__unormalize_form*: "
				"working buffer too large\n"));
			return (idn_nomemory);
		}
		goto again;
	default:
		return (r);
	}
	/* NOTREACHED */
}

static void		
get_class(workbuf_t *wb) {
	int i;

	for (i = wb->cur; i < wb->last; i++)
		wb->class[i] = idn__unicode_canonicalclass(wb->version,
							   wb->ucs4[i]);
}

static void
reorder(workbuf_t *wb) {
	unsigned long c;
	int i;
	int class;

	assert(wb != NULL);

	i = wb->cur;
	c = wb->ucs4[i];
	class = wb->class[i];

	while (i > 0 && wb->class[i - 1] > class) {
		wb->ucs4[i] = wb->ucs4[i - 1];
		wb->class[i] =wb->class[i - 1];
		i--;
		wb->ucs4[i] = c;
		wb->class[i] = class;
	}
}

static void
compose(workbuf_t *wb) {
	int cur;
	unsigned long *ucs4;
	int *class;
	int last_class;
	int nvoids;
	int i;
	idn__unicode_version_t ver;

	assert(wb != NULL && wb->class[0] == 0);

	cur = wb->cur;
	ucs4 = wb->ucs4;
	class = wb->class;
	ver = wb->version;

	/*
	 * If there are no decomposition sequence that begins with
	 * the top character, composition is impossible.
	 */
	if (!idn__unicode_iscompositecandidate(ver, ucs4[0]))
		return;

	last_class = 0;
	nvoids = 0;
	for (i = 1; i <= cur; i++) {
		unsigned long c;
		int cl = class[i];

		if ((last_class < cl || cl == 0) &&
		    idn__unicode_compose(ver, ucs4[0], ucs4[i],
					 &c) == idn_success) {
			/*
			 * Replace the top character with the composed one.
			 */
			ucs4[0] = c;
			class[0] = idn__unicode_canonicalclass(ver, c);

			class[i] = -1;	/* void this character */
			nvoids++;
		} else {
			last_class = cl;
		}
	}

	/* Purge void characters, if any. */
	if (nvoids > 0)
		workbuf_removevoid(wb);
}

static idn_result_t
flush_before_cur(workbuf_t *wb, unsigned long **top, size_t *tolenp) {
	if (*tolenp < wb->cur)
		return (idn_buffer_overflow);

	memcpy(*top, wb->ucs4, sizeof(**top) * wb->cur);
	*top += wb->cur;
	*tolenp -= wb->cur;
	workbuf_shift(wb, wb->cur);

	return (idn_success);
}

static void
workbuf_init(workbuf_t *wb) {
	wb->cur = 0;
	wb->last = 0;
	wb->size = WORKBUF_SIZE;
	wb->ucs4 = wb->ucs4_buf;
	wb->class = wb->class_buf;
}

static void
workbuf_free(workbuf_t *wb) {
	if (wb->ucs4 != wb->ucs4_buf) {
		free(wb->ucs4);
		free(wb->class);
	}
}

static idn_result_t
workbuf_extend(workbuf_t *wb) {
	int newsize = wb->size * 3;

	if (wb->ucs4 == wb->ucs4_buf) {
		wb->ucs4 = malloc(sizeof(wb->ucs4[0]) * newsize);
		wb->class = malloc(sizeof(wb->class[0]) * newsize);
	} else {
		wb->ucs4 = realloc(wb->ucs4, sizeof(wb->ucs4[0]) * newsize);
		wb->class = realloc(wb->class, sizeof(wb->class[0]) * newsize);
	}
	if (wb->ucs4 == NULL || wb->class == NULL)
		return (idn_nomemory);
	else
		return (idn_success);
}

static idn_result_t
workbuf_append(workbuf_t *wb, unsigned long c) {
	idn_result_t r;

	if (wb->last >= wb->size && (r = workbuf_extend(wb)) != idn_success)
		return (r);
	wb->ucs4[wb->last++] = c;
	return (idn_success);
}

static void
workbuf_shift(workbuf_t *wb, int shift) {
	int nmove;

	assert(wb != NULL && wb->cur >= shift);

	nmove = wb->last - shift;
	(void)memmove(&wb->ucs4[0], &wb->ucs4[shift],
		      nmove * sizeof(wb->ucs4[0]));
	(void)memmove(&wb->class[0], &wb->class[shift],
		      nmove * sizeof(wb->class[0]));
	wb->cur -= shift;
	wb->last -= shift;
}

static void
workbuf_removevoid(workbuf_t *wb) {
	int i, j;
	int last = wb->last;

	for (i = j = 0; i < last; i++) {
		if (wb->class[i] >= 0) {
			if (j < i) {
				wb->ucs4[j] = wb->ucs4[i];
				wb->class[j] = wb->class[i];
			}
			j++;
		}
	}
	wb->cur -= last - j;
	wb->last = j;
}
