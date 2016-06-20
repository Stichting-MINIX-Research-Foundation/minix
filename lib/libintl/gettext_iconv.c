/*	$NetBSD: gettext_iconv.c,v 1.8 2009/02/18 13:08:22 yamt Exp $	*/

/*-
 * Copyright (c) 2004 Citrus Project,
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
 *
 * $Citrus$
 */


#include <sys/types.h>
#include <sys/param.h>

#include <errno.h>
#include <iconv.h>
#include <libintl.h>
#include <langinfo.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

#include "libintl_local.h"

struct cache {
	const char *c_origmsg;
	const char *c_resultmsg;
};

static const struct cache *cache_find(const char *, struct domainbinding *);
static int cache_enter(const char *, const char *);
static int cache_cmp(const void *, const void *);

static void *cacheroot;

/* ARGSUSED1 */
static const struct cache *
cache_find(const char *msg, struct domainbinding *db)
{
	struct cache key;
	struct cache **c;

	key.c_origmsg = msg;
	c = tfind(&key, &cacheroot, cache_cmp);

	return c ? *c : NULL;
}

static int
cache_enter(const char *origmsg, const char *resultmsg)
{
	struct cache *c;

	c = malloc(sizeof(*c));
	if (c == NULL)
		return -1;

	c->c_origmsg = origmsg;
	c->c_resultmsg = resultmsg;

	if (tsearch(c, &cacheroot, cache_cmp) == NULL) {
		free(c);
		return -1;
	}

	return 0;
}

static int
cache_cmp(const void *va, const void *vb)
{
	const struct cache *a = va;
	const struct cache *b = vb;
	int result;

	if (a->c_origmsg > b->c_origmsg) {
		result = 1;
	} else if (a->c_origmsg < b->c_origmsg) {
		result = -1;
	} else {
		result = 0;
	}

	return result;
}

#define	GETTEXT_ICONV_MALLOC_CHUNK	(16 * 1024)

const char *
__gettext_iconv(const char *origmsg, struct domainbinding *db)
{
	const char *tocode;
	const char *fromcode = db->mohandle.mo.mo_charset;
	const struct cache *cache;
	const char *result;
	iconv_t cd;
	const char *src;
	char *dst;
	size_t origlen;
	size_t srclen;
	size_t dstlen;
	size_t nvalid;
	int savederrno = errno;

	/*
	 * static buffer for converted texts.
	 *
	 * note:
	 * we never free buffers once returned to callers.
	 * because of interface design of gettext, we can't know
	 * the lifetime of them.
	 */
	static char *buffer;
	static size_t bufferlen;

	/*
	 * don't convert message if *.mo doesn't specify codeset.
	 */
	if (fromcode == NULL)
		return origmsg;

	tocode = db->codeset;
	if (tocode == NULL) {
		/*
		 * codeset isn't specified explicitly by
		 * bind_textdomain_codeset().
		 * use current locale(LC_CTYPE)'s codeset.
		 *
		 * XXX maybe wrong; it can mismatch with
		 * environment variable setting.
		 */
		tocode = nl_langinfo(CODESET);
	}

	/*
	 * shortcut if possible.
	 * XXX should handle aliases
	 */
	if (!strcasecmp(tocode, fromcode))
		return origmsg;

	/* XXX LOCK */

	/* XXX should detect change of tocode and purge caches? */

	/*
	 * see if we have already converted this message.
	 */
	cache = cache_find(origmsg, db);
	if (cache) {
		result = cache->c_resultmsg;
		goto out;
	}

	origlen = strlen(origmsg) + 1;
again:
	cd = iconv_open(tocode, fromcode);
	if (cd == (iconv_t)-1) {
		result = origmsg;
		goto out;
	}

	src = origmsg;
	srclen = origlen;
	dst = buffer;
	dstlen = bufferlen;
	nvalid = iconv(cd, &src, &srclen, &dst, &dstlen);
	iconv_close(cd);

	if (nvalid == (size_t)-1) {
		/*
		 * try to allocate a new buffer.
		 *
		 * just give up if GETTEXT_ICONV_MALLOC_CHUNK was not enough.
		 */
		if (errno == E2BIG &&
		    bufferlen != GETTEXT_ICONV_MALLOC_CHUNK) {
			buffer = malloc(GETTEXT_ICONV_MALLOC_CHUNK);
			if (buffer) {
				bufferlen = GETTEXT_ICONV_MALLOC_CHUNK;
				goto again;
			}
		}

		result = origmsg;
	} else if (cache_enter(origmsg, buffer)) {
		/*
		 * failed to enter cache.  give up.
		 */
		result = origmsg;
	} else {
		size_t resultlen = dst - buffer;

		result = buffer;
		bufferlen -= resultlen;
		buffer += resultlen;
	}

out:
	/* XXX UNLOCK */
	errno = savederrno;

	return result;
}
