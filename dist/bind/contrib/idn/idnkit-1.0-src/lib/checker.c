#ifndef lint
static char *rcsid = "$Id: checker.c,v 1.1.1.1 2003-06-04 00:25:49 marka Exp $";
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
#include <idn/checker.h>
#include <idn/strhash.h>
#include <idn/debug.h>

/*
 * Type for checking scheme.
 */
typedef struct {
	char *prefix;
	char *parameter;
	idn_checker_createproc_t create;
	idn_checker_destroyproc_t destroy;
	idn_checker_lookupproc_t lookup;
	void *context;
} check_scheme_t;

/*
 * Standard checking schemes.
 */
static const check_scheme_t rfc3491_prohibit_scheme = {
	"prohibit#RFC3491",
	"RFC3491",
	idn_nameprep_createproc,
	idn_nameprep_destroyproc,
	idn_nameprep_prohibitproc,
	NULL,
};

static const check_scheme_t rfc3491_unasigned_scheme = {
	"unassigned#RFC3491",
	"RFC3491",
	idn_nameprep_createproc,
	idn_nameprep_destroyproc,
	idn_nameprep_unassignedproc,
	NULL,
};

static const check_scheme_t rfc3491_bidi_scheme = {
	"bidi#RFC3491",
	"RFC3491",
	idn_nameprep_createproc,
	idn_nameprep_destroyproc,
	idn_nameprep_bidiproc,
	NULL,
};

static const check_scheme_t filecheck_prohibit_scheme = {
	"prohibit#fileset",
	NULL,
	idn__filechecker_createproc,
	idn__filechecker_destroyproc,
	idn__filechecker_lookupproc,
	NULL,
};

static const check_scheme_t filecheck_unassigned_scheme = {
	"unassigned#fileset",
	NULL,
	idn__filechecker_createproc,
	idn__filechecker_destroyproc,
	idn__filechecker_lookupproc,
	NULL,
};

static const check_scheme_t *standard_check_schemes[] = {
	&rfc3491_unasigned_scheme,
	&rfc3491_prohibit_scheme,
	&rfc3491_bidi_scheme,
	&filecheck_prohibit_scheme,
	&filecheck_unassigned_scheme,
	NULL,
};

/*
 * Hash table for checking schemes.
 */
static idn__strhash_t scheme_hash = NULL;

/*
 * Mapper object type.
 */
struct idn_checker {
	int nschemes;
	int scheme_size;
	check_scheme_t *schemes;
	int reference_count;
};

#define MAPPER_INITIAL_SCHEME_SIZE	1

idn_result_t
idn_checker_initialize(void) {
	idn_result_t r;
	check_scheme_t **scheme;

	TRACE(("idn_checker_initialize()\n"));

	if (scheme_hash != NULL) {
		r = idn_success;	/* already initialized */
		goto ret;
	}

	r = idn__strhash_create(&scheme_hash);
	if (r != idn_success) {
		goto ret;
	}

	for (scheme = (check_scheme_t **)standard_check_schemes;
		*scheme != NULL; scheme++) {
		r = idn__strhash_put(scheme_hash, (*scheme)->prefix, *scheme);
		if (r != idn_success)
			goto ret;
	}

	r = idn_success;
ret:
	if (r != idn_success) {
		if (scheme_hash != NULL) {
			idn__strhash_destroy(scheme_hash, NULL);
			scheme_hash = NULL;
		}
	}
	TRACE(("idn_checker_initialize(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_checker_create(idn_checker_t *ctxp) {
	idn_checker_t ctx = NULL;
	idn_result_t r;

	assert(scheme_hash != NULL);
	assert(ctxp != NULL);

	TRACE(("idn_checker_create()\n"));

	ctx = (idn_checker_t) malloc(sizeof(struct idn_checker));
	if (ctx == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->schemes = (check_scheme_t *) malloc(sizeof(check_scheme_t)
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
	TRACE(("idn_checker_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_checker_destroy(idn_checker_t ctx) {
	int i;

	assert(scheme_hash != NULL);
	assert(ctx != NULL);

	TRACE(("idn_checker_destroy()\n"));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		TRACE(("idn_checker_destroy(): the object is destroyed\n"));
		for (i = 0; i < ctx->nschemes; i++)
			ctx->schemes[i].destroy(ctx->schemes[i].context);
		free(ctx->schemes);
		free(ctx);
	} else {
		TRACE(("idn_checker_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_checker_incrref(idn_checker_t ctx) {
	assert(ctx != NULL && scheme_hash != NULL);

	TRACE(("idn_checker_incrref()\n"));
	TRACE(("idn_checker_incrref: update reference count (%d->%d)\n",
		ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

idn_result_t
idn_checker_add(idn_checker_t ctx, const char *scheme_name) {
	idn_result_t r;
	check_scheme_t *scheme;
	const char *scheme_prefix;
	const char *scheme_parameter;
	void *scheme_context = NULL;
	char *buffer = NULL;

	assert(scheme_hash != NULL);
	assert(ctx != NULL);

	TRACE(("idn_checker_add(scheme_name=%s)\n",
		idn__debug_xstring(scheme_name, 50)));

	/*
	 * Split `scheme_name' into `scheme_prefix' and `scheme_parameter'.
	 */
	scheme_parameter = strchr(scheme_name, ':');
	if (scheme_parameter == NULL) {
		scheme_prefix = scheme_name;
		scheme_parameter = NULL;
	} else {
		ptrdiff_t scheme_prefixlen;

		scheme_prefixlen = scheme_parameter - scheme_name;
		buffer = (char *) malloc(scheme_prefixlen + 1);
		if (buffer == NULL) {
			r = idn_nomemory;
			goto ret;
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
		ERROR(("idn_checker_add(): invalid scheme \"%-.30s\"\n",
		       scheme_name));
		r = idn_invalid_name;
		goto ret;
	}
	if (scheme_parameter == NULL && scheme->parameter != NULL)
		scheme_parameter = scheme->parameter;

	/*
	 * Add the scheme.
	 */
	assert(ctx->nschemes <= ctx->scheme_size);

	if (ctx->nschemes == ctx->scheme_size) {
		check_scheme_t *new_schemes;

		new_schemes = (check_scheme_t *) realloc(ctx->schemes,
			sizeof(check_scheme_t) * ctx->scheme_size * 2);
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

	memcpy(ctx->schemes + ctx->nschemes, scheme, sizeof(check_scheme_t));
	ctx->schemes[ctx->nschemes].context = scheme_context;
	ctx->nschemes++;
	r = idn_success;

ret:
	free(buffer);
	if (r != idn_success)
		free(scheme_context);
	TRACE(("idn_checker_add(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_checker_addall(idn_checker_t ctx, const char **scheme_names,
		   int nschemes) {
	idn_result_t r;
	int i;

	assert(scheme_hash != NULL);
	assert(ctx != NULL && scheme_names != NULL);

	TRACE(("idn_checker_addall(nschemes=%d)\n", nschemes));

	for (i = 0; i < nschemes; i++) {
		r = idn_checker_add(ctx, (const char *)*scheme_names);
		if (r != idn_success)
			goto ret;
		scheme_names++;
	}

	r = idn_success;
ret:
	TRACE(("idn_checker_addall(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_checker_lookup(idn_checker_t ctx, const unsigned long *ucs4,
		   const unsigned long **found) {
	idn_result_t r;
	int i;

	assert(scheme_hash != NULL);
	assert(ctx != NULL && ucs4 != NULL && found != NULL);

	TRACE(("idn_checker_lookup(ucs4=\"%s\")\n",
		idn__debug_ucs4xstring(ucs4, 50)));

	/*
	 * Lookup.
	 */
	*found = NULL;

	for (i = 0; i < ctx->nschemes; i++) {
		TRACE(("idn_checker_lookup(): lookup %s\n",
		       ctx->schemes[i].prefix));

		r = (ctx->schemes[i].lookup)(ctx->schemes[i].context, ucs4,
					     found);
		if (r != idn_success)
			goto ret;
		if (*found != NULL)
			break;
	}

	r = idn_success;
ret:
	if (*found == NULL) {
		TRACE(("idn_checker_lookup(): %s (not found)\n",
		       idn_result_tostring(r)));
	} else {
		TRACE(("idn_checker_lookup(): %s (found \\x%04lx)\n",
		       idn_result_tostring(r), **found));
	}
	return (r);
}

idn_result_t
idn_checker_register(const char *prefix,		    
		    idn_checker_createproc_t create,
		    idn_checker_destroyproc_t destroy,
		    idn_checker_lookupproc_t lookup) {
	idn_result_t r;
	check_scheme_t *scheme = NULL;

	assert(scheme_hash != NULL);
	assert(prefix != NULL && create != NULL && destroy != NULL &&
		lookup != NULL);

	TRACE(("idn_checker_register(prefix=%s)\n", prefix));

	scheme = (check_scheme_t *) malloc(sizeof(check_scheme_t));
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
	scheme->lookup    = lookup;

	r = idn__strhash_put(scheme_hash, prefix, scheme);
ret:
	if (r != idn_success) {
		if (scheme != NULL)
			free(scheme->prefix);
		free(scheme);
	}
	TRACE(("idn_checker_register(): %s\n", idn_result_tostring(r)));
	return (r);
}
