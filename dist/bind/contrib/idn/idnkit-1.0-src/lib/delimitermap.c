#ifndef lint
static char *rcsid = "$Id: delimitermap.c,v 1.1.1.1 2003-06-04 00:25:52 marka Exp $";
#endif

/*
 * Copyright (c) 2001,2002 Japan Network Information Center. 
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
#include <idn/delimitermap.h>
#include <idn/util.h>
#include <idn/debug.h>
#include <idn/ucs4.h>

/*
 * Mapper object type.
 */
struct idn_delimitermap {
	int ndelimiters;
	int delimiter_size;
	unsigned long *delimiters;
	int reference_count;
};

#define DELIMITERMAP_INITIAL_DELIMITER_SIZE	4
#define UNICODE_MAX		0x10ffff
#define IS_SURROGATE_HIGH(v)	(0xd800 <= (v) && (v) <= 0xdbff)
#define IS_SURROGATE_LOW(v)	(0xdc00 <= (v) && (v) <= 0xdfff)

idn_result_t
idn_delimitermap_create(idn_delimitermap_t *ctxp) {
	idn_delimitermap_t ctx = NULL;
	idn_result_t r;

	assert(ctxp != NULL);
	TRACE(("idn_delimitermap_create()\n"));

	ctx = (idn_delimitermap_t) malloc(sizeof(struct idn_delimitermap));
	if (ctx == NULL) {
		WARNING(("idn_mapper_create: malloc failed\n"));
		r = idn_nomemory;
		goto ret;
	}

	ctx->delimiters = (unsigned long *) malloc(sizeof(unsigned long)
		* DELIMITERMAP_INITIAL_DELIMITER_SIZE);
	if (ctx->delimiters == NULL) {
		r = idn_nomemory;
		goto ret;
	}
	ctx->ndelimiters = 0;
	ctx->delimiter_size = DELIMITERMAP_INITIAL_DELIMITER_SIZE;
	ctx->reference_count = 1;
	*ctxp = ctx;
	r = idn_success;

ret:
	if (r != idn_success)
		free(ctx);
	TRACE(("idn_delimitermap_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_delimitermap_destroy(idn_delimitermap_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_delimitermap_destroy()\n"));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		TRACE(("idn_mapper_destroy(): the object is destroyed\n"));
		free(ctx->delimiters);
		free(ctx);
	} else {
		TRACE(("idn_delimitermap_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_delimitermap_incrref(idn_delimitermap_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_delimitermap_incrref()\n"));
	TRACE(("idn_delimitermap_incrref: update reference count (%d->%d)\n",
		ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

idn_result_t
idn_delimitermap_add(idn_delimitermap_t ctx, unsigned long delimiter) {
	idn_result_t r;

	assert(ctx != NULL && ctx->ndelimiters <= ctx->delimiter_size);
	TRACE(("idn_delimitermap_add(delimiter=\\x%04lx)\n", delimiter));

	if (delimiter == 0 || delimiter > UNICODE_MAX ||
	    IS_SURROGATE_HIGH(delimiter) || IS_SURROGATE_LOW(delimiter)) {
		r = idn_invalid_codepoint;
		goto ret;
	}
	    
	if (ctx->ndelimiters == ctx->delimiter_size) {
		unsigned long *new_delimiters;

		new_delimiters = (unsigned long *) realloc(ctx->delimiters,
			sizeof(unsigned long) * ctx->delimiter_size * 2);
		if (new_delimiters == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		ctx->delimiters = new_delimiters;
		ctx->delimiter_size *= 2;
	}

	ctx->delimiters[ctx->ndelimiters] = delimiter;
	ctx->ndelimiters++;
	r = idn_success;

ret:
	TRACE(("idn_delimitermap_add(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_delimitermap_addall(idn_delimitermap_t ctx, unsigned long *delimiters,
			int ndelimiters) {
	idn_result_t r;
	int i;

	assert(ctx != NULL && delimiters != NULL);

	TRACE(("idn_delimitermap_addall(ndelimiters=%d)\n", ndelimiters));

	for (i = 0; i < ndelimiters; i++) {
		r = idn_delimitermap_add(ctx, *delimiters);
		if (r != idn_success)
			goto ret;
		delimiters++;
	}

	r = idn_success;
ret:
	TRACE(("idn_delimitermap_addall(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_delimitermap_map(idn_delimitermap_t ctx, const unsigned long *from, 
		     unsigned long *to, size_t tolen) {

	/* default delimiters (label separators) from IDNA specification */
	static const unsigned long default_delimiters[] =
		{ 0x002e, /* full stop */
		  0x3002, /* ideographic full stop */
		  0xff0e, /* fullwidth full stop */
		  0xff61, /* halfwidth ideographic full stop */
		  0x0000 };

	unsigned long *to_org = to;
	idn_result_t r;
	int i, j;
	int found;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_delimitermap_map(from=\"%s\", tolen=%d)\n",
		idn__debug_ucs4xstring(from, 50), (int)tolen));

	/*
	 * Map.
	 */
	while (*from != '\0') {
		found = 0;
		if (tolen < 1) {
			r = idn_buffer_overflow;
			goto ret;
		}
		for (j = 0; default_delimiters[j] != 0x0000; j++) {
			if (default_delimiters[j] == *from) {
				found = 1;
				break;
			}
		}
		if (!found) {
			for (i = 0; i < ctx->ndelimiters; i++) {
				if (ctx->delimiters[i] == *from) {
					found = 1;
					break;
				}
			}
		}
		if (found)
			*to = '.';
		else
			*to = *from;
		from++;	
		to++;
		tolen--;
	}

	if (tolen < 1) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*to = '\0';
	r = idn_success;

ret:
	if (r == idn_success) {
		TRACE(("idn_delimitermap_map(): success (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to_org, 50)));
	} else {
		TRACE(("idn_delimitermap_map(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}
