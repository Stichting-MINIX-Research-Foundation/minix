#ifndef lint
static char *rcsid = "$Id: normalizer.c,v 1.1.1.1 2003-06-04 00:26:05 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
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
#include <ctype.h>

#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/result.h>
#include <idn/normalizer.h>
#include <idn/strhash.h>
#include <idn/unormalize.h>
#include <idn/unicode.h>
#include <idn/ucs4.h>
#include <idn/debug.h>
#include <idn/util.h>

#define MAX_LOCAL_SCHEME	3

#define INITIALIZED		(scheme_hash != NULL)

typedef struct {
	char *name;
	idn_normalizer_proc_t proc;
} normalize_scheme_t;

struct idn_normalizer {
	int nschemes;
	int scheme_size;
	normalize_scheme_t **schemes;
	normalize_scheme_t *local_buf[MAX_LOCAL_SCHEME];
	int reference_count;
};

static idn__strhash_t scheme_hash;

static idn__unicode_version_t vcur = NULL;
static idn__unicode_version_t v320 = NULL;
#define INIT_VERSION(version, var) \
	if (var == NULL) { \
		idn_result_t r = idn__unicode_create(version, &var); \
		if (r != idn_success) \
			return (r); \
	}

static idn_result_t	expand_schemes(idn_normalizer_t ctx);
static idn_result_t	register_standard_normalizers(void);
static idn_result_t	normalizer_formkc(const unsigned long *from,
					  unsigned long *to, size_t tolen);
static idn_result_t	normalizer_formkc_v320(const unsigned long *from,
					       unsigned long *to,
					       size_t tolen);

static struct standard_normalizer {
	char *name;
	idn_normalizer_proc_t proc;
} standard_normalizer[] = {
	{ "unicode-form-kc", normalizer_formkc },
	{ "unicode-form-kc/3.2.0", normalizer_formkc_v320 },
	{ "RFC3491", normalizer_formkc_v320 },
	{ NULL, NULL },
};

idn_result_t
idn_normalizer_initialize(void) {
	idn__strhash_t hash;
	idn_result_t r;

	TRACE(("idn_normalizer_initialize()\n"));

	if (scheme_hash != NULL) {
		r = idn_success;	/* already initialized */
		goto ret;
	}

	if ((r = idn__strhash_create(&hash)) != idn_success)
		goto ret;
	scheme_hash = hash;

	/* Register standard normalizers */
	r = register_standard_normalizers();
ret:
	TRACE(("idn_normalizer_initialize(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_normalizer_create(idn_normalizer_t *ctxp) {
	idn_normalizer_t ctx;
	idn_result_t r;

	assert(ctxp != NULL);
	TRACE(("idn_normalizer_create()\n"));

	if ((ctx = malloc(sizeof(struct idn_normalizer))) == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->nschemes = 0;
	ctx->scheme_size = MAX_LOCAL_SCHEME;
	ctx->schemes = ctx->local_buf;
	ctx->reference_count = 1;
	*ctxp = ctx;

	r = idn_success;
ret:
	TRACE(("idn_normalizer_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_normalizer_destroy(idn_normalizer_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_normalizer_destroy()\n"));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		TRACE(("idn_normalizer_destroy(): the object is destroyed\n"));
		if (ctx->schemes != ctx->local_buf)
			free(ctx->schemes);
		free(ctx);
	} else {
		TRACE(("idn_normalizer_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_normalizer_incrref(idn_normalizer_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_normalizer_incrref()\n"));
	TRACE(("idn_normalizer_incrref: update reference count (%d->%d)\n",
	    ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

idn_result_t
idn_normalizer_add(idn_normalizer_t ctx, const char *scheme_name) {
	idn_result_t r;
	void *v;
	normalize_scheme_t *scheme;

	assert(ctx != NULL && scheme_name != NULL);

	TRACE(("idn_normalizer_add(scheme_name=%s)\n", scheme_name));

	assert(INITIALIZED);

	if (idn__strhash_get(scheme_hash, scheme_name, &v) != idn_success) {
		ERROR(("idn_normalizer_add(): invalid scheme \"%-.30s\"\n",
		       scheme_name));
		r = idn_invalid_name;
		goto ret;
	}

	scheme = v;

	assert(ctx->nschemes <= ctx->scheme_size);

	if (ctx->nschemes == ctx->scheme_size &&
	    (r = expand_schemes(ctx)) != idn_success) {
		goto ret;
	}

	ctx->schemes[ctx->nschemes++] = scheme;
	r = idn_success;
ret:
	TRACE(("idn_normalizer_add(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_normalizer_addall(idn_normalizer_t ctx, const char **scheme_names,
		      int nschemes) {
	idn_result_t r;
	int i;

	assert(ctx != NULL && scheme_names != NULL);

	TRACE(("idn_normalizer_addall(nschemes=%d)\n", nschemes));

	for (i = 0; i < nschemes; i++) {
		r = idn_normalizer_add(ctx, (const char *)*scheme_names);
		if (r != idn_success)
			goto ret;
		scheme_names++;
	}

	r = idn_success;
ret:
	TRACE(("idn_normalizer_addall(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_normalizer_normalize(idn_normalizer_t ctx, const unsigned long *from,
			 unsigned long *to, size_t tolen) {
	idn_result_t r;
	unsigned long *src, *dst;
	unsigned long *buffers[2] = {NULL, NULL};
	size_t buflen[2] = {0, 0};
	size_t dstlen;
	int idx;
	int i;

	assert(scheme_hash != NULL);
	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_normalizer_normalize(from=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(from, 50), (int)tolen));

	if (ctx->nschemes <= 0) {
		if (tolen < idn_ucs4_strlen(from) + 1) {
			r = idn_buffer_overflow;
			goto ret;
		}
		idn_ucs4_strcpy(to, from);
		r = idn_success;
		goto ret;
	}

	/*
	 * Normalize.
	 */
	src = (void *)from;
	dstlen = idn_ucs4_strlen(from) + 1;

	i = 0;
	while (i < ctx->nschemes) {
		TRACE(("idn_normalizer_normalize(): normalize %s\n",
		       ctx->schemes[i]->name));

		/*
		 * Choose destination area to restore the result of a mapping.
		 */
		if (i + 1 == ctx->nschemes) {
			dst = to;
			dstlen = tolen;
		} else {
			if (src == buffers[0])
				idx = 1;
			else
				idx = 0;

			if (buflen[idx] < dstlen) {
				void *newbuf;

				newbuf = realloc(buffers[idx],
						 sizeof(long) * dstlen);
				if (newbuf == NULL) {
					r = idn_nomemory;
					goto ret;
				}
				buffers[idx] = (unsigned long *)newbuf;
				buflen[idx] = dstlen;
			}

			dst = buffers[idx];
			dstlen = buflen[idx];
		}

		/*
		 * Perform i-th normalization scheme.
		 * If buffer size is not enough, we double it and try again.
		 */
		r = (ctx->schemes[i]->proc)(src, dst, dstlen);
		if (r == idn_buffer_overflow && dst != to) {
			dstlen *= 2;
			continue;
		}
		if (r != idn_success)
			goto ret;

		src = dst;
		i++;
	}

	r = idn_success;
ret:
	free(buffers[0]);
	free(buffers[1]);
	if (r == idn_success) {
		TRACE(("idn_normalizer_normalize(): success (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to, 50)));
	} else {
		TRACE(("idn_normalizer_normalize(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_normalizer_register(const char *scheme_name, idn_normalizer_proc_t proc) {
	idn_result_t r;
	normalize_scheme_t *scheme;

	assert(scheme_name != NULL && proc != NULL);

	TRACE(("idn_normalizer_register(scheme_name=%s)\n", scheme_name));

	assert(INITIALIZED);

	scheme = malloc(sizeof(*scheme) + strlen(scheme_name) + 1);
	if (scheme == NULL) {
		r = idn_nomemory;
		goto ret;
	}
	scheme->name = (char *)(scheme + 1);
	(void)strcpy(scheme->name, scheme_name);
	scheme->proc = proc;

	r = idn__strhash_put(scheme_hash, scheme_name, scheme);
	if (r != idn_success)
		goto ret;

	r = idn_success;
ret:
	TRACE(("idn_normalizer_register(): %s\n", idn_result_tostring(r)));
	return (r);
}

static idn_result_t
expand_schemes(idn_normalizer_t ctx) {
	normalize_scheme_t **new_schemes;
	int new_size = ctx->scheme_size * 2;

	if (ctx->schemes == ctx->local_buf) {
		new_schemes = malloc(sizeof(normalize_scheme_t) * new_size);
	} else {
		new_schemes = realloc(ctx->schemes,
				      sizeof(normalize_scheme_t) * new_size);
	}
	if (new_schemes == NULL)
		return (idn_nomemory);

	if (ctx->schemes == ctx->local_buf)
		memcpy(new_schemes, ctx->local_buf, sizeof(ctx->local_buf));

	ctx->schemes = new_schemes;
	ctx->scheme_size = new_size;

	return (idn_success);
}

static idn_result_t
register_standard_normalizers(void) {
	int i;
	int failed = 0;

	for (i = 0; standard_normalizer[i].name != NULL; i++) {
		idn_result_t r;
		r = idn_normalizer_register(standard_normalizer[i].name,
					    standard_normalizer[i].proc);
		if (r != idn_success) {
			WARNING(("idn_normalizer_initialize(): "
				"failed to register \"%-.100s\"\n",
				standard_normalizer[i].name));
			failed++;
		}
	}
	if (failed > 0)
		return (idn_failure);
	else
		return (idn_success);
}

/*
 * Unicode Normalization Forms -- latest version
 */

static idn_result_t
normalizer_formkc(const unsigned long *from, unsigned long *to, size_t tolen) {
	INIT_VERSION(NULL, vcur);
	return (idn__unormalize_formkc(vcur, from, to, tolen));
}

/*
 * Unicode Normalization Forms -- version 3.2.0
 */

static idn_result_t
normalizer_formkc_v320(const unsigned long *from, unsigned long *to,
		       size_t tolen) {
	INIT_VERSION("3.2.0", v320);
	return (idn__unormalize_formkc(v320, from, to, tolen));
}
