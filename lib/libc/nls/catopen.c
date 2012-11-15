/*	$NetBSD: catopen.c,v 1.31 2012/07/30 23:02:41 yamt Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: catopen.c,v 1.31 2012/07/30 23:02:41 yamt Exp $");
#endif /* LIBC_SCCS and not lint */

#define _NLS_PRIVATE

#include "namespace.h"
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <nl_types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "citrus_namespace.h"
#include "citrus_bcs.h"
#include "citrus_region.h"
#include "citrus_lookup.h"
#include "citrus_aliasname_local.h"

#define NLS_ALIAS_DB "/usr/share/nls/nls.alias"

#define NLS_DEFAULT_PATH "/usr/share/nls/%L/%N.cat:/usr/share/nls/%N/%L"
#define NLS_DEFAULT_LANG "C"

#ifdef __weak_alias
__weak_alias(catopen, _catopen)
#endif

static nl_catd load_msgcat(const char *);

nl_catd
_catopen(const char *name, int oflag)
{
	char tmppath[PATH_MAX+1];
	const char *nlspath;
	const char *lang, *reallang;
	char *t;
	const char *s, *u;
	nl_catd catd;
	char langbuf[PATH_MAX];

	if (name == NULL || *name == '\0')
		return (nl_catd)-1;

	/* absolute or relative path? */
	if (strchr(name, '/'))
		return load_msgcat(name);

	if (issetugid() || (nlspath = getenv("NLSPATH")) == NULL)
		nlspath = NLS_DEFAULT_PATH;
	/*
	 * histrical note:
	 * http://www.hauN.org/ml/b-l-j/a/800/828.html (in japanese)
	 */
	if (oflag == NL_CAT_LOCALE) {
		lang = setlocale(LC_MESSAGES, NULL);
	} else {
		lang = getenv("LANG");
	}
	if (lang == NULL || strchr(lang, '/'))
		lang = NLS_DEFAULT_LANG;

	reallang = __unaliasname(NLS_ALIAS_DB, lang, langbuf, sizeof(langbuf));
	if (reallang == NULL)
		reallang = lang;

	s = nlspath;
	t = tmppath;
	do {
		while (*s && *s != ':') {
			if (*s == '%') {
				switch (*(++s)) {
				case 'L':	/* locale */
					u = reallang;
					while (*u && t < tmppath + PATH_MAX)
						*t++ = *u++;
					break;
				case 'N':	/* name */
					u = name;
					while (*u && t < tmppath + PATH_MAX)
						*t++ = *u++;
					break;
				case 'l':	/* lang */
				case 't':	/* territory */
				case 'c':	/* codeset */
					break;
				default:
					if (t < tmppath + PATH_MAX)
						*t++ = *s;
				}
			} else {
				if (t < tmppath + PATH_MAX)
					*t++ = *s;
			}
			s++;
		}

		*t = '\0';
		catd = load_msgcat(tmppath);
		if (catd != (nl_catd)-1)
			return catd;

		if (*s)
			s++;
		t = tmppath;
	} while (*s);

	return (nl_catd)-1;
}

static nl_catd
load_msgcat(const char *path)
{
	struct stat st;
	nl_catd catd;
	void *data;
	int fd;

	_DIAGASSERT(path != NULL);

	if ((fd = open(path, O_RDONLY)) == -1)
		return (nl_catd)-1;

	if (fstat(fd, &st) != 0) {
		close (fd);
		return (nl_catd)-1;
	}

#ifdef __minix
	if(!(data = malloc((size_t)st.st_size))) {
		return (nl_catd)-1;
	}

	if (read(fd, data, st.st_size) != st.st_size)
	{
		free(data);
		return (nl_catd)-1;
	}
	close (fd);
#else /* !__minix */
	data = mmap(0, (size_t)st.st_size, PROT_READ, MAP_FILE|MAP_SHARED, fd,
	    (off_t)0);
	close (fd);

	if (data == MAP_FAILED) {
		return (nl_catd)-1;
	}
#endif /* __minix */

	if (ntohl((u_int32_t)((struct _nls_cat_hdr *)data)->__magic) !=
	    _NLS_MAGIC) {
#ifdef __minix
		free(data);
#else
		munmap(data, (size_t)st.st_size);
#endif
		return (nl_catd)-1;
	}

	if ((catd = malloc(sizeof (*catd))) == NULL) {
#ifdef __minix
		free(data);
#else
		munmap(data, (size_t)st.st_size);
#endif
		return (nl_catd)-1;
	}

	catd->__data = data;
	catd->__size = (int)st.st_size;
	return catd;
}
