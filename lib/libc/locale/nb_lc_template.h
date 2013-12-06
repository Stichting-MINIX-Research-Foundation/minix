/* $NetBSD: nb_lc_template.h,v 1.8 2013/09/13 13:13:32 joerg Exp $ */

/*-
 * Copyright (c)1999, 2008 Citrus Project,
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NB_LC_TEMPLATE_H_
#define _NB_LC_TEMPLATE_H_

#define _nb_part_t		_PREFIX(part_t)
#define _nb_part_cache		_PREFIX(part_cache)
#define _nb_mutex		_PREFIX(mutex)

typedef struct _nb_part_t {
	char name[_LOCALENAME_LEN_MAX];
	_CATEGORY_TYPE *impl;
	SIMPLEQ_ENTRY(_nb_part_t) entry;
} _nb_part_t;

static SIMPLEQ_HEAD(, _nb_part_t) _nb_part_cache =
    SIMPLEQ_HEAD_INITIALIZER(_nb_part_cache);

#ifdef _REENTRANT
static mutex_t _nb_mutex = MUTEX_INITIALIZER;
#endif

static int
_PREFIX(load_sub)(const char * __restrict name, const char * __restrict real,
    const char ** __restrict out_name, _CATEGORY_TYPE ** __restrict out_impl,
    int force)
{
	const char *cached_name;
	_CATEGORY_TYPE *cached_impl;
	_nb_part_t *p;
	int ret;

	_DIAGASSERT(name != NULL);
	_DIAGASSERT(out_name != NULL);
	_DIAGASSERT(out_impl != NULL);

	if (!strcmp(_C_LOCALE, name)) {
		cached_name = _lc_C_locale.part_name[_CATEGORY_ID];
		cached_impl = _lc_C_locale.part_impl[_CATEGORY_ID];
	} else if (!strcmp(_POSIX_LOCALE, name)) {
		cached_name = _POSIX_LOCALE;
		cached_impl = _lc_C_locale.part_impl[_CATEGORY_ID];
	} else {
		SIMPLEQ_FOREACH(p, &_nb_part_cache, entry) {
			if (!strcmp((const char *)&p->name[0], name)) {
				cached_name = p->name;
				cached_impl = p->impl;
				goto found;
			}
		}
		p = malloc(sizeof(*p));
		if (p == NULL)
			return ENOMEM;
		if (force) {
			p->impl = _lc_C_locale.part_impl[_CATEGORY_ID];
		} else {
			_DIAGASSERT(_PathLocale != NULL);
			ret = _PREFIX(create_impl)((const char *)_PathLocale,
			    name, &p->impl);
			if (ret) {
				free(p);
				return ret;
			}
		}
		strlcpy(&p->name[0], name, sizeof(p->name));
		SIMPLEQ_INSERT_TAIL(&_nb_part_cache, p, entry);
		cached_name = p->name;
		cached_impl = p->impl;
	}
found:
	if (real != NULL) {
		p = malloc(sizeof(*p));
		if (p == NULL)
			return ENOMEM;
		strlcpy(&p->name[0], real, sizeof(p->name));
		cached_name = p->name;
		p->impl = cached_impl;
		SIMPLEQ_INSERT_TAIL(&_nb_part_cache, p, entry);
	}
	*out_name = cached_name;
	*out_impl = cached_impl;
	return 0;
}

static __inline int
_PREFIX(load)(const char * __restrict name,
    const char ** __restrict out_name, _CATEGORY_TYPE ** __restrict out_impl)
{
	int ret, force;
	char path[PATH_MAX + 1], loccat[PATH_MAX + 1], buf[PATH_MAX + 1];
	const char *aliaspath, *alias;

#define _LOAD_SUB_ALIAS(key)						\
do {									\
	alias = __unaliasname(aliaspath, key, &buf[0], sizeof(buf));	\
	if (alias != NULL) {						\
		ret = (force = !__isforcemapping(alias))		\
		    ? _PREFIX(load_sub)(name, NULL, out_name, out_impl, \
				        force)	\
		    : _PREFIX(load_sub)(alias, name, out_name, out_impl, \
				        force);	\
		_DIAGASSERT(!ret || !force);				\
		goto done;						\
	}								\
} while (/*CONSTCOND*/0)

	/* (1) non-aliased file */
	mutex_lock(&_nb_mutex);
	ret = _PREFIX(load_sub)(name, NULL, out_name, out_impl, 0);
	if (ret != ENOENT)
		goto done;

	/* (2) lookup locname/catname type alias */
	_DIAGASSERT(_PathLocale != NULL);
	snprintf(&path[0], sizeof(path),
	    "%s/" _LOCALE_ALIAS_NAME, _PathLocale);
	aliaspath = (const char *)&path[0];
	snprintf(&loccat[0], sizeof(loccat),
	    "%s/" _CATEGORY_NAME, name);
	_LOAD_SUB_ALIAS((const char *)&loccat[0]);

	/* (3) lookup locname type alias */
	_LOAD_SUB_ALIAS(name);

done:
	mutex_unlock(&_nb_mutex);
	return ret;
}

const char *
_PREFIX(setlocale)(const char * __restrict name,
    struct _locale * __restrict locale)
{
	const char *loaded_name;
	_CATEGORY_TYPE *loaded_impl;

	/* name may be NULL */
	_DIAGASSERT(locale != NULL);

	if (name != NULL) {
		if (*name == '\0')
			name = _get_locale_env(_CATEGORY_NAME);
		_DIAGASSERT(name != NULL);
		_DIAGASSERT(locale->part_name[(size_t)_CATEGORY_ID] != NULL);
		if (strcmp(name, locale->part_name[(size_t)_CATEGORY_ID])) {
			if (_PREFIX(load)(name, &loaded_name, &loaded_impl))
				return NULL;
			locale->part_name[(size_t)_CATEGORY_ID] = loaded_name;
			locale->part_impl[(size_t)_CATEGORY_ID] = loaded_impl;
			if (locale == &_lc_global_locale)
				_PREFIX(update_global)(loaded_impl);
		}
	}
	return locale->part_name[(size_t)_CATEGORY_ID];
}

#endif /*_NB_LC_TEMPLATE_H_*/
