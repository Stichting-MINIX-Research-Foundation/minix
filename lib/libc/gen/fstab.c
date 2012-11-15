/*	$NetBSD: fstab.c,v 1.31 2012/03/13 21:13:34 christos Exp $	*/

/*
 * Copyright (c) 1980, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)fstab.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: fstab.c,v 1.31 2012/03/13 21:13:34 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(endfsent,_endfsent)
__weak_alias(getfsent,_getfsent)
__weak_alias(getfsfile,_getfsfile)
__weak_alias(getfsspec,_getfsspec)
__weak_alias(setfsent,_setfsent)
#endif

static FILE *_fs_fp;
static size_t _fs_lineno = 0;
static const char *_fs_file = _PATH_FSTAB;
static struct fstab _fs_fstab;

static char *nextfld(char **, const char *);
static int fstabscan(void);


static char *
nextfld(char **str, const char *sep)
{
	char *ret;

	_DIAGASSERT(str != NULL);
	_DIAGASSERT(sep != NULL);

	while ((ret = stresep(str, sep, '\\')) != NULL && *ret == '\0')
		continue;
	return ret;
}


static int
fstabscan(void)
{
	char *cp, *lp, *sp;
#define	MAXLINELENGTH	1024
	static char line[MAXLINELENGTH];
	char subline[MAXLINELENGTH];
	static const char sep[] = ":\n";
	static const char ws[] = " \t\n";
	static const char *fstab_type[] = {
	    FSTAB_RW, FSTAB_RQ, FSTAB_RO, FSTAB_SW, FSTAB_DP, FSTAB_XX, NULL 
	};

	(void)memset(&_fs_fstab, 0, sizeof(_fs_fstab));
	for (;;) {
		if (!(lp = fgets(line, (int)sizeof(line), _fs_fp)))
			return 0;
		_fs_lineno++;
/* OLD_STYLE_FSTAB */
		if (!strpbrk(lp, " \t")) {
			_fs_fstab.fs_spec = nextfld(&lp, sep);
			if (!_fs_fstab.fs_spec || *_fs_fstab.fs_spec == '#')
				continue;
			_fs_fstab.fs_file = nextfld(&lp, sep);
			_fs_fstab.fs_type = nextfld(&lp, sep);
			if (_fs_fstab.fs_type) {
				if (!strcmp(_fs_fstab.fs_type, FSTAB_XX))
					continue;
				_fs_fstab.fs_mntops = _fs_fstab.fs_type;
				_fs_fstab.fs_vfstype =
				    __UNCONST(
				    strcmp(_fs_fstab.fs_type, FSTAB_SW) ?
				    "ufs" : "swap");
				if ((cp = nextfld(&lp, sep)) != NULL) {
					_fs_fstab.fs_freq = atoi(cp);
					if ((cp = nextfld(&lp, sep)) != NULL) {
						_fs_fstab.fs_passno = atoi(cp);
						return 1;
					}
				}
			}
			goto bad;
		}
/* OLD_STYLE_FSTAB */
		_fs_fstab.fs_spec = nextfld(&lp, ws);
		if (!_fs_fstab.fs_spec || *_fs_fstab.fs_spec == '#')
			continue;
		_fs_fstab.fs_file = nextfld(&lp, ws);
		_fs_fstab.fs_vfstype = nextfld(&lp, ws);
		_fs_fstab.fs_mntops = nextfld(&lp, ws);
		if (_fs_fstab.fs_mntops == NULL)
			goto bad;
		_fs_fstab.fs_freq = 0;
		_fs_fstab.fs_passno = 0;
		if ((cp = nextfld(&lp, ws)) != NULL) {
			_fs_fstab.fs_freq = atoi(cp);
			if ((cp = nextfld(&lp, ws)) != NULL)
				_fs_fstab.fs_passno = atoi(cp);
		}

		/* subline truncated iff line truncated */
		(void)strlcpy(subline, _fs_fstab.fs_mntops, sizeof(subline));
		sp = subline;

		while ((cp = nextfld(&sp, ",")) != NULL) {
			const char **tp;

			if (strlen(cp) != 2)
				continue;

			for (tp = fstab_type; *tp; tp++)
				if (strcmp(cp, *tp) == 0) {
					_fs_fstab.fs_type = __UNCONST(*tp);
					break;
				}
			if (*tp)
				break;
		}
		if (_fs_fstab.fs_type == NULL)
			goto bad;
		if (strcmp(_fs_fstab.fs_type, FSTAB_XX) == 0)
			continue;
		if (cp != NULL)
			return 1;

bad:
		warnx("%s, %lu: Missing fields", _fs_file, (u_long)_fs_lineno);
	}
	/* NOTREACHED */
}

struct fstab *
getfsent(void)
{
	if ((!_fs_fp && !setfsent()) || !fstabscan())
		return NULL;
	return &_fs_fstab;
}

struct fstab *
getfsspec(const char *name)
{

	_DIAGASSERT(name != NULL);

	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_spec, name))
				return &_fs_fstab;
	return NULL;
}

struct fstab *
getfsfile(const char *name)
{

	_DIAGASSERT(name != NULL);

	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_file, name))
				return &_fs_fstab;
	return NULL;
}

int
setfsent(void)
{
	_fs_lineno = 0;
	if (_fs_fp) {
		rewind(_fs_fp);
		return 1;
	}
	if ((_fs_fp = fopen(_PATH_FSTAB, "re")) == NULL) {
		warn("Cannot open `%s'", _PATH_FSTAB);
		return 0;
	}
	return 1;
}

void
endfsent(void)
{
	if (_fs_fp) {
		(void)fclose(_fs_fp);
		_fs_fp = NULL;
	}
}
