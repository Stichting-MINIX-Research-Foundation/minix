/*	$NetBSD: citrus_iconv.c,v 1.10 2011/11/19 18:34:21 tnozaki Exp $	*/

/*-
 * Copyright (c)2003 Citrus Project,
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: citrus_iconv.c,v 1.10 2011/11/19 18:34:21 tnozaki Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"

#include <sys/types.h>
#include <sys/queue.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "citrus_namespace.h"
#include "citrus_bcs.h"
#include "citrus_region.h"
#include "citrus_memstream.h"
#include "citrus_mmap.h"
#include "citrus_module.h"
#include "citrus_lookup.h"
#include "citrus_hash.h"
#include "citrus_iconv.h"

#define _CITRUS_ICONV_DIR	"iconv.dir"
#define _CITRUS_ICONV_ALIAS	"iconv.alias"

#define CI_HASH_SIZE 101
#define CI_INITIAL_MAX_REUSE	5
#define CI_ENV_MAX_REUSE	"ICONV_MAX_REUSE"

#ifdef _REENTRANT
static rwlock_t lock = RWLOCK_INITIALIZER;
#endif

static bool isinit = false;
static _CITRUS_HASH_HEAD(, _citrus_iconv_shared, CI_HASH_SIZE) shared_pool;
static TAILQ_HEAD(, _citrus_iconv_shared) shared_unused;
static int shared_num_unused, shared_max_reuse;

static __inline void
init_cache(void)
{
	rwlock_wrlock(&lock);
	if (!isinit) {
		_CITRUS_HASH_INIT(&shared_pool, CI_HASH_SIZE);
		TAILQ_INIT(&shared_unused);
		shared_max_reuse = -1;
		if (!issetugid() && getenv(CI_ENV_MAX_REUSE))
			shared_max_reuse = atoi(getenv(CI_ENV_MAX_REUSE));
		if (shared_max_reuse < 0)
			shared_max_reuse = CI_INITIAL_MAX_REUSE;
		isinit = true;
	}
	rwlock_unlock(&lock);
}

/*
 * lookup_iconv_entry:
 *	lookup iconv.dir entry in the specified directory.
 *
 * line format of iconv.dir file:
 *	key  module  arg
 * key    : lookup key.
 * module : iconv module name.
 * arg    : argument for the module (generally, description file name)
 *
 */
static __inline int
lookup_iconv_entry(const char *curdir, const char *key,
		   char *linebuf, size_t linebufsize,
		   const char **module, const char **variable)
{
	const char *cp, *cq;
	char *p, path[PATH_MAX];

	/* iconv.dir path */
	snprintf(path, (size_t)PATH_MAX, ("%s/" _CITRUS_ICONV_DIR), curdir);

	/* lookup db */
	cp = p = _lookup_simple(path, key, linebuf, linebufsize,
				_LOOKUP_CASE_IGNORE);
	if (p == NULL)
		return ENOENT;

	/* get module name */
	*module = p;
	cq = _bcs_skip_nonws(cp);
	p[cq-cp] = '\0';
	p += cq-cp+1;
	cq++;

	/* get variable */
	cp = _bcs_skip_ws(cq);
	*variable = p += cp - cq;
	cq = _bcs_skip_nonws(cp);
	p[cq-cp] = '\0';

	return 0;
}

static __inline void
close_shared(struct _citrus_iconv_shared *ci)
{
	if (ci) {
		if (ci->ci_module) {
			if (ci->ci_ops) {
				if (ci->ci_closure)
					(*ci->ci_ops->io_uninit_shared)(ci);
				free(ci->ci_ops);
			}
			_citrus_unload_module(ci->ci_module);
		}
		free(ci);
	}
}

static __inline int
open_shared(struct _citrus_iconv_shared * __restrict * __restrict rci,
	    const char * __restrict basedir, const char * __restrict convname,
	    const char * __restrict src, const char * __restrict dst)
{
	int ret;
	struct _citrus_iconv_shared *ci;
	_citrus_iconv_getops_t getops;
	char linebuf[LINE_MAX];
	const char *module, *variable;
	size_t len_convname;

	/* search converter entry */
	ret = lookup_iconv_entry(basedir, convname, linebuf, sizeof(linebuf),
				 &module, &variable);
	if (ret) {
		if (ret == ENOENT)
			/* fallback */
			ret = lookup_iconv_entry(basedir, "*",
						 linebuf, sizeof(linebuf),
						 &module, &variable);
		if (ret)
			return ret;
	}

	/* initialize iconv handle */
	len_convname = strlen(convname);
	ci = malloc(sizeof(*ci)+len_convname+1);
	if (!ci) {
		ret = errno;
		goto err;
	}
	ci->ci_module = NULL;
	ci->ci_ops = NULL;
	ci->ci_closure = NULL;
	ci->ci_convname = (void *)&ci[1];
	memcpy(ci->ci_convname, convname, len_convname+1);

	/* load module */
	ret = _citrus_load_module(&ci->ci_module, module);
	if (ret)
		goto err;

	/* get operators */
	getops = (_citrus_iconv_getops_t)
	    _citrus_find_getops(ci->ci_module, module, "iconv");
	if (!getops) {
		ret = EOPNOTSUPP;
		goto err;
	}
	ci->ci_ops = malloc(sizeof(*ci->ci_ops));
	if (!ci->ci_ops) {
		ret = errno;
		goto err;
	}
	ret = (*getops)(ci->ci_ops, sizeof(*ci->ci_ops),
			_CITRUS_ICONV_ABI_VERSION);
	if (ret)
		goto err;

	/* version check */
	if (ci->ci_ops->io_abi_version == 1) {
		/* binary compatibility broken at ver.2 */
		ret = EINVAL;
		goto err;
	}

	if (ci->ci_ops->io_init_shared == NULL ||
	    ci->ci_ops->io_uninit_shared == NULL ||
	    ci->ci_ops->io_init_context == NULL ||
	    ci->ci_ops->io_uninit_context == NULL ||
	    ci->ci_ops->io_convert == NULL) {
		ret = EINVAL;
		goto err;
	}

	/* initialize the converter */
	ret = (*ci->ci_ops->io_init_shared)(ci, basedir, src, dst,
					    (const void *)variable,
					    strlen(variable)+1);
	if (ret)
		goto err;

	*rci = ci;

	return 0;
err:
	close_shared(ci);
	return ret;
}

static __inline int
hash_func(const char *key)
{
	return _string_hash_func(key, CI_HASH_SIZE);
}

static __inline int
match_func(struct _citrus_iconv_shared * __restrict ci,
	   const char * __restrict key)
{
	return strcmp(ci->ci_convname, key);
}

static int
get_shared(struct _citrus_iconv_shared * __restrict * __restrict rci,
	   const char *basedir, const char *src, const char *dst)
{
	int ret = 0;
	int hashval;
	struct _citrus_iconv_shared * ci;
	char convname[PATH_MAX];

	snprintf(convname, sizeof(convname), "%s/%s", src, dst);

	rwlock_wrlock(&lock);

	/* lookup alread existing entry */
	hashval = hash_func(convname);
	_CITRUS_HASH_SEARCH(&shared_pool, ci, ci_hash_entry, match_func,
			    convname, hashval);
	if (ci != NULL) {
		/* found */
		if (ci->ci_used_count == 0) {
			TAILQ_REMOVE(&shared_unused, ci, ci_tailq_entry);
			shared_num_unused--;
		}
		ci->ci_used_count++;
		*rci = ci;
		goto quit;
	}

	/* create new entry */
	ret = open_shared(&ci, basedir, convname, src, dst);
	if (ret)
		goto quit;

	_CITRUS_HASH_INSERT(&shared_pool, ci, ci_hash_entry, hashval);
	ci->ci_used_count = 1;
	*rci = ci;

quit:
	rwlock_unlock(&lock);

	return ret;
}

static void
release_shared(struct _citrus_iconv_shared * __restrict ci)
{
	rwlock_wrlock(&lock);

	ci->ci_used_count--;
	if (ci->ci_used_count == 0) {
		/* put it into unused list */
		shared_num_unused++;
		TAILQ_INSERT_TAIL(&shared_unused, ci, ci_tailq_entry);
		/* flood out */
		while (shared_num_unused > shared_max_reuse) {
			ci = TAILQ_FIRST(&shared_unused);
			_DIAGASSERT(ci != NULL);
			TAILQ_REMOVE(&shared_unused, ci, ci_tailq_entry);
			_CITRUS_HASH_REMOVE(ci, ci_hash_entry);
			shared_num_unused--;
			close_shared(ci);
		}
	}

	rwlock_unlock(&lock);
}

/*
 * _citrus_iconv_open:
 *	open a converter for the specified in/out codes.
 */
int
_citrus_iconv_open(struct _citrus_iconv * __restrict * __restrict rcv,
		   const char * __restrict basedir,
		   const char * __restrict src, const char * __restrict dst)
{
	int ret;
	struct _citrus_iconv_shared *ci = NULL;
	struct _citrus_iconv *cv;
	char realsrc[PATH_MAX], realdst[PATH_MAX];
	char buf[PATH_MAX], path[PATH_MAX];

	init_cache();

	/* resolve codeset name aliases */
	snprintf(path, sizeof(path), "%s/%s", basedir, _CITRUS_ICONV_ALIAS);
	strlcpy(realsrc,
		_lookup_alias(path, src, buf, (size_t)PATH_MAX,
		_LOOKUP_CASE_IGNORE),
		(size_t)PATH_MAX);
	strlcpy(realdst,
		_lookup_alias(path, dst, buf, (size_t)PATH_MAX,
		_LOOKUP_CASE_IGNORE),
		(size_t)PATH_MAX);

	/* sanity check */
	if (strchr(realsrc, '/') != NULL || strchr(realdst, '/'))
		return EINVAL;

	/* get shared record */
	ret = get_shared(&ci, basedir, realsrc, realdst);
	if (ret)
		return ret;

	/* create/init context */
	cv = malloc(sizeof(*cv));
	if (cv == NULL) {
		ret = errno;
		release_shared(ci);
		return ret;
	}
	cv->cv_shared = ci;
	ret = (*ci->ci_ops->io_init_context)(cv);
	if (ret) {
		release_shared(ci);
		free(cv);
		return ret;
	}
	*rcv = cv;

	return 0;
}

/*
 * _citrus_iconv_close:
 *	close the specified converter.
 */
void
_citrus_iconv_close(struct _citrus_iconv *cv)
{
	if (cv) {
		(*cv->cv_shared->ci_ops->io_uninit_context)(cv);
		release_shared(cv->cv_shared);
		free(cv);
	}
}
