#ifndef lint
static char *rcsid = "$Id: mapper.c,v 1.1.1.1 2003-06-04 00:25:55 marka Exp $";
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
#include <idn/mapper.h>
#include <idn/strhash.h>
#include <idn/debug.h>
#include <idn/util.h>
#include <idn/ucs4.h>

/*
 * Type for mapping scheme.
 */
typedef struct {
	char *prefix;
	char *parameter;
	idn_mapper_createproc_t create;
	idn_mapper_destroyproc_t destroy;
	idn_mapper_mapproc_t map;
	void *context;
} map_scheme_t;

/*
 * Standard mapping schemes.
 */
static const map_scheme_t nameprep_scheme = {
	"RFC3491",
	NULL,
	idn_nameprep_createproc,
	idn_nameprep_destroyproc,
	idn_nameprep_mapproc,
	NULL,
};

static const map_scheme_t filemap_scheme = {
	"filemap",
	"",
	idn__filemapper_createproc,
	idn__filemapper_destroyproc,
	idn__filemapper_mapproc,
	NULL,
};

static const map_scheme_t *standard_map_schemes[] = {
	&nameprep_scheme,
	&filemap_scheme,
	NULL,
};

/*
 * Hash table for mapping schemes.
 */
static idn__strhash_t scheme_hash = NULL;

/*
 * Mapper object type.
 */
struct idn_mapper {
	int nschemes;
	int scheme_size;
	map_scheme_t *schemes;
	int reference_count;
};

#define MAPPER_INITIAL_SCHEME_SIZE	1

idn_result_t
idn_mapper_initialize(void) {
	idn_result_t r;
	map_scheme_t **scheme;

	TRACE(("idn_mapper_initialize()\n"));

	if (scheme_hash != NULL) {
		r = idn_success;	/* already initialized */
		goto ret;
	}

	r = idn__strhash_create(&scheme_hash);
	if (r != idn_success)
		goto ret;

	for (scheme = (map_scheme_t **)standard_map_schemes;
		*scheme != NULL; scheme++) {
		r = idn__strhash_put(scheme_hash, (*scheme)->prefix, *scheme);
		if (r != idn_success)
			goto ret;
	}

	r = idn_success;
ret:
	if (r != idn_success && scheme_hash != NULL) {
		idn__strhash_destroy(scheme_hash, NULL);
		scheme_hash = NULL;
	}
	TRACE(("idn_mapper_initialize(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_mapper_create(idn_mapper_t *ctxp) {
	idn_mapper_t ctx = NULL;
	idn_result_t r;

	assert(scheme_hash != NULL);
	assert(ctxp != NULL);

	TRACE(("idn_mapper_create()\n"));

	ctx = (idn_mapper_t) malloc(sizeof(struct idn_mapper));
	if (ctx == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->schemes = (map_scheme_t *) malloc(sizeof(map_scheme_t)
		 * MAPPER_INITIAL_SCHEME_SIZE);
	if (ctx->schemes == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->nschemes = 0;
	ctx->scheme_size = MAPPER_INITIAL_SCHEME_SIZE;
	ctx->reference_count = 1;
	*ctxp = ctx;
	r = idn_success;

ret:
	if (r != idn_success) {
		if (ctx != NULL)
			free(ctx->schemes);
		free(ctx);
	}
	TRACE(("idn_mapper_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_mapper_destroy(idn_mapper_t ctx) {
	int i;

	assert(scheme_hash != NULL);
	assert(ctx != NULL);

	TRACE(("idn_mapper_destroy()\n"));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		TRACE(("idn_mapper_destroy(): the object is destroyed\n"));
		for (i = 0; i < ctx->nschemes; i++)
			ctx->schemes[i].destroy(ctx->schemes[i].context);
		free(ctx->schemes);
		free(ctx);
	} else {
		TRACE(("idn_mapper_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_mapper_incrref(idn_mapper_t ctx) {
	assert(ctx != NULL && scheme_hash != NULL);

	TRACE(("idn_mapper_incrref()\n"));
	TRACE(("idn_mapper_incrref: update reference count (%d->%d)\n",
		ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

idn_result_t
idn_mapper_add(idn_mapper_t ctx, const char *scheme_name) {
	idn_result_t r;
	map_scheme_t *scheme;
	const char *scheme_prefix;
	const char *scheme_parameter;
	void *scheme_context = NULL;
	char static_buffer[128];	/* large enough */
	char *buffer = static_buffer;

	assert(scheme_hash != NULL);
	assert(ctx != NULL);

	TRACE(("idn_mapper_add(scheme_name=%s)\n",
		idn__debug_xstring(scheme_name, 50)));

	/*
	 * Split `scheme_name' into `scheme_prefix' and `scheme_parameter'.
	 */
	scheme_parameter = strchr(scheme_name, ':');
	if (scheme_parameter == NULL) {
		scheme_prefix = scheme_name;
	} else {
		ptrdiff_t scheme_prefixlen;

		scheme_prefixlen = scheme_parameter - scheme_name;
		if (scheme_prefixlen + 1 > sizeof(static_buffer)) {
			buffer = (char *) malloc(scheme_prefixlen + 1);
			if (buffer == NULL) {
				r = idn_nomemory;
				goto ret;
			}
		}
		memcpy(buffer, scheme_name, scheme_prefixlen);
		*(buffer + scheme_prefixlen) = '\0';
		scheme_prefix = buffer;
		scheme_parameter++;
	}

	/*
	 * Find a scheme.
	 */
	if (idn__strhash_get(scheme_hash, scheme_prefix, (void **)&scheme)
		!= idn_success) {
		ERROR(("idn_mapper_add(): invalid scheme name \"%-.30s\"\n",
		       scheme_prefix));
		r = idn_invalid_name;
		goto ret;
	}
	if (scheme_parameter == NULL) {
		if (scheme->parameter != NULL)
			scheme_parameter = scheme->parameter;
		else
			scheme_parameter = scheme->prefix;
	}

	/*
	 * Add the scheme.
	 */
	assert(ctx->nschemes <= ctx->scheme_size);

	if (ctx->nschemes == ctx->scheme_size) {
		map_scheme_t *new_schemes;

		new_schemes = (map_scheme_t *) realloc(ctx->schemes,
			sizeof(map_scheme_t) * ctx->scheme_size * 2);
		if (new_schemes == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		ctx->schemes = new_schemes;
		ctx->scheme_size *= 2;
	}

	r = scheme->create(scheme_parameter, &scheme_context);
	if (r != idn_success)
		goto ret;

	memcpy(ctx->schemes + ctx->nschemes, scheme, sizeof(map_scheme_t));
	ctx->schemes[ctx->nschemes].context = scheme_context;
	ctx->nschemes++;
	r = idn_success;
ret:
	if (r != idn_success)
		free(scheme_context);
	if (buffer != static_buffer)
		free(buffer);
	TRACE(("idn_mapper_add(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_mapper_addall(idn_mapper_t ctx, const char **scheme_names, int nschemes) {
	idn_result_t r;
	int i;

	assert(scheme_hash != NULL);
	assert(ctx != NULL && scheme_names != NULL);

	TRACE(("idn_mapper_addall(nschemes=%d)\n", nschemes));

	for (i = 0; i < nschemes; i++) {
		r = idn_mapper_add(ctx, (const char *)*scheme_names);
		if (r != idn_success)
			goto ret;
		scheme_names++;
	}

	r = idn_success;
ret:
	TRACE(("idn_mapper_addall(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_mapper_map(idn_mapper_t ctx, const unsigned long *from,
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

	TRACE(("idn_mapper_map(from=\"%s\", tolen=%d)\n",
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
	 * Map.
	 */
	src = (void *)from;
	dstlen = idn_ucs4_strlen(from) + 1;

	i = 0;
	while (i < ctx->nschemes) {
		TRACE(("idn_mapper_map(): map %s\n", ctx->schemes[i].prefix));

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
		 * Perform i-th map scheme.
		 * If buffer size is not enough, we double it and try again.
		 */
		r = (ctx->schemes[i].map)(ctx->schemes[i].context, src, dst,
					  dstlen);
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
		TRACE(("idn_mapper_map(): success (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to, 50)));
	} else {
		TRACE(("idn_mapper_map(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_mapper_register(const char *prefix,		    
		    idn_mapper_createproc_t create,
		    idn_mapper_destroyproc_t destroy,
		    idn_mapper_mapproc_t map) {
	idn_result_t r;
	map_scheme_t *scheme = NULL;

	assert(scheme_hash != NULL);
	assert(prefix != NULL && create != NULL && destroy != NULL &&
		map != NULL);

	TRACE(("idn_mapper_register(prefix=%s)\n", prefix));

	scheme = (map_scheme_t *) malloc(sizeof(map_scheme_t));
	if (scheme == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	scheme->prefix = (char *) malloc(strlen(prefix) + 1);
	if (scheme->prefix == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	strcpy(scheme->prefix, prefix);
	scheme->parameter = NULL;
	scheme->create    = create;
	scheme->destroy   = destroy;
	scheme->map       = map;

	r = idn__strhash_put(scheme_hash, prefix, scheme);
	if (r != idn_success)
		goto ret;

	r = idn_success;
ret:
	if (r != idn_success) {
		if (scheme != NULL)
			free(scheme->prefix);
		free(scheme);
	}

	TRACE(("idn_mapper_register(): %s\n", idn_result_tostring(r)));
	return (r);
}
