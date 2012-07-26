#ifndef lint
static char *rcsid = "$Id: mapselector.c,v 1.1.1.1 2003-06-04 00:25:56 marka Exp $";
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

#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/result.h>
#include <idn/mapselector.h>
#include <idn/strhash.h>
#include <idn/debug.h>
#include <idn/util.h>
#include <idn/ucs4.h>

struct idn_mapselector {
	idn__strhash_t maphash;
	int reference_count;
};

/*
 * Maximum length of a top level domain name. (e.g. `com', `jp', ...)
 */
#define MAPSELECTOR_MAX_TLD_LENGTH	63

static void string_ascii_tolower(char *string);


const unsigned long *
idn_mapselector_getnotld(void) {
	static const unsigned long notld[] = {0x002d, 0x0000};  /* "-" */
	return (notld);
}

const unsigned long *
idn_mapselector_getdefaulttld(void) {
	const static unsigned long defaulttld[] = {0x002e, 0x0000};  /* "." */
	return (defaulttld);
}

idn_result_t
idn_mapselector_initialize(void) {
	idn_result_t r;

	TRACE(("idn_mapselector_initialize()\n"));

	r = idn_mapper_initialize();

	TRACE(("idn_mapselector_initialize(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_mapselector_create(idn_mapselector_t *ctxp) {
	idn_mapselector_t ctx = NULL;
	idn_result_t r;

	assert(ctxp != NULL);
	TRACE(("idn_mapselector_create()\n"));

	ctx = (idn_mapselector_t)malloc(sizeof(struct idn_mapselector));
	if (ctx == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	ctx->maphash = NULL;
	ctx->reference_count = 1;

	r = idn__strhash_create(&(ctx->maphash));
	if (r != idn_success)
		goto ret;

	*ctxp = ctx;
	r = idn_success;

ret:
	if (r != idn_success) {
		if (ctx != NULL)
			free(ctx->maphash);
		free(ctx);
	}
	TRACE(("idn_mapselector_create(): %s\n", idn_result_tostring(r)));
	return (r);
}

void
idn_mapselector_destroy(idn_mapselector_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_mapselector_destroy()\n"));

	ctx->reference_count--;
	if (ctx->reference_count <= 0) {
		TRACE(("idn_mapselector_destroy(): "
		       "the object is destroyed\n"));
		idn__strhash_destroy(ctx->maphash,
			(idn__strhash_freeproc_t)&idn_mapper_destroy);
		free(ctx);
	} else {
		TRACE(("idn_mapselector_destroy(): "
		       "update reference count (%d->%d)\n",
		       ctx->reference_count + 1, ctx->reference_count));
	}
}

void
idn_mapselector_incrref(idn_mapselector_t ctx) {
	assert(ctx != NULL);

	TRACE(("idn_mapselector_incrref()\n"));
	TRACE(("idn_mapselector_incrref: update reference count (%d->%d)\n",
		ctx->reference_count, ctx->reference_count + 1));

	ctx->reference_count++;
}

idn_result_t
idn_mapselector_add(idn_mapselector_t ctx, const char *tld, const char *name) {
	idn_result_t r;
	idn_mapper_t mapper;
	char hash_key[MAPSELECTOR_MAX_TLD_LENGTH + 1];

	assert(ctx != NULL && tld != NULL);

	TRACE(("idn_mapselector_add(tld=%s, name=%s)\n", tld, name));

	if (!(tld[0] == '.' && tld[1] == '\0')) {
		if (tld[0] == '.')
			tld++;
		if (strchr(tld, '.') != NULL) {
			ERROR(("idn_mapselector_add: "
			       "invalid TLD \"%-.100s\"\n", tld));
			r = idn_invalid_name;
			goto ret;
		}
	}
	if (strlen(tld) > MAPSELECTOR_MAX_TLD_LENGTH) {
		ERROR(("idn_mapselector_add: "
		       "too long TLD \"%-.100s\"\n", tld));
		r = idn_invalid_name;
		goto ret;
	}
	strcpy(hash_key, tld);
	string_ascii_tolower(hash_key);

	if (idn__strhash_get(ctx->maphash, hash_key, (void **)&mapper)
		!= idn_success) {
		r = idn_mapper_create(&mapper);
		if (r != idn_success)
			goto ret;

		r = idn__strhash_put(ctx->maphash, hash_key, mapper);
		if (r != idn_success)
			goto ret;
	}

	r = idn_mapper_add(mapper, name);
ret:
	TRACE(("idn_mapselector_add(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_mapselector_addall(idn_mapselector_t ctx, const char *tld,
		       const char **scheme_names, int nschemes) {
	idn_result_t r;
	int i;

	assert(ctx != NULL && tld != NULL && scheme_names != NULL);

	TRACE(("idn_mapselector_addall(tld=%s, nschemes=%d)\n", 
	      tld, nschemes));

	for (i = 0; i < nschemes; i++) {
		r = idn_mapselector_add(ctx, tld, (const char *)*scheme_names);
		if (r != idn_success)
			goto ret;
		scheme_names++;
	}

	r = idn_success;
ret:
	TRACE(("idn_mapselector_addall(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_mapper_t
idn_mapselector_mapper(idn_mapselector_t ctx, const char *tld) {
	idn_result_t r;
	idn_mapper_t mapper;
	char hash_key[MAPSELECTOR_MAX_TLD_LENGTH + 1];

	assert(ctx != NULL && tld != NULL);

	TRACE(("idn_mapselector_mapper(tld=%s)\n", tld));

	if (!(tld[0] == '.' && tld[1] == '\0')) {
		if (tld[0] == '.')
			tld++;
		if (strchr(tld, '.') != NULL)
			return (NULL);
	}
	if (strlen(tld) > MAPSELECTOR_MAX_TLD_LENGTH)
		return (NULL);
	strcpy(hash_key, tld);
	string_ascii_tolower(hash_key);

	mapper = NULL;
	r = idn__strhash_get(ctx->maphash, hash_key, (void **)&mapper);
	if (r != idn_success)
		return (NULL);

	idn_mapper_incrref(mapper);

	return (mapper);
}

idn_result_t
idn_mapselector_map(idn_mapselector_t ctx, const unsigned long *from,
		    const char *tld, unsigned long *to, size_t tolen) {
	idn_result_t r;
	idn_mapper_t mapper = NULL;
	char hash_key[MAPSELECTOR_MAX_TLD_LENGTH + 1];
	size_t fromlen;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_mapselector_map(from=\"%s\", tld=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(from, 50), idn__debug_xstring(tld, 50),
	       (int)tolen));

	if (!(tld[0] == '.' && tld[1] == '\0')) {
		if (tld[0] == '.')
			tld++;
		if (strchr(tld, '.') != NULL) {
			r = idn_invalid_name;
			goto ret;
		}
	}
	if (strlen(tld) > MAPSELECTOR_MAX_TLD_LENGTH) {
		r = idn_invalid_name;
		goto ret;
	}
	strcpy(hash_key, tld);
	string_ascii_tolower(hash_key);

	fromlen = idn_ucs4_strlen(from);

	/*
	 * Get mapper for the TLD.
	 */
	if (idn__strhash_get(ctx->maphash, hash_key, (void **)&mapper)
	    != idn_success) {
		strcpy(hash_key, IDN_MAPSELECTOR_DEFAULTTLD);
		idn__strhash_get(ctx->maphash, hash_key, (void **)&mapper);
	}

	/*
	 * Map.
	 * If default mapper has not been registered, copy the string.
	 */
	if (mapper == NULL) {
		TRACE(("idn_mapselector_map(): no mapper\n"));
		if (fromlen + 1 > tolen) {
			r = idn_buffer_overflow;
			goto ret;
		}
		memcpy(to, from, (fromlen + 1) * sizeof(*from));
	} else {
		TRACE(("idn_mapselector_map(): tld=%s\n", tld));
		r = idn_mapper_map(mapper, from, to, tolen);
		if (r != idn_success)
			goto ret;
	}

	r = idn_success;
ret:
	if (r == idn_success) {
		TRACE(("idn_mapselector_map(): succcess (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to, 50)));
	} else {
		TRACE(("idn_mapselector_map(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_mapselector_map2(idn_mapselector_t ctx, const unsigned long *from,
		     const unsigned long *tld, unsigned long *to,
		     size_t tolen) {
	char tld_utf8[MAPSELECTOR_MAX_TLD_LENGTH + 1];
	idn_result_t r;

	assert(ctx != NULL && from != NULL && to != NULL);

	TRACE(("idn_mapselector_map2(from=\"%s\", tld=\"%s\")\n",
	       idn__debug_ucs4xstring(from, 50),
	       idn__debug_ucs4xstring(tld, 50)));

	r = idn_ucs4_ucs4toutf8(tld, tld_utf8, sizeof(tld_utf8));
	if (r == idn_buffer_overflow) {
		r = idn_invalid_name;
		goto ret;
	} else if (r != idn_success) {
		goto ret;
	}

	r = idn_mapselector_map(ctx, from, tld_utf8, to, tolen);
ret:
	if (r == idn_success) {
		TRACE(("idn_mapselector_map2(): success (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to, 50)));
	} else {
	    TRACE(("idn_mapselector_map2(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

static void
string_ascii_tolower(char *string) {
	unsigned char *p;

	for (p = (unsigned char *) string; *p != '\0'; p++) {
		if ('A' <= *p && *p <= 'Z')
			*p = *p - 'A' + 'a';
	}
}
