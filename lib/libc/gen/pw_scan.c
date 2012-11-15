/*	$NetBSD: pw_scan.c,v 1.23 2012/03/13 21:13:36 christos Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994, 1995
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#include "compat_pwd.h"

#else
#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: pw_scan.c,v 1.23 2012/03/13 21:13:36 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#if defined(_LIBC)
#include "namespace.h"
#endif
#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef _LIBC
#include "pw_private.h"
#endif
#endif /* ! HAVE_NBTOOL_CONFIG_H */

static int
gettime(time_t *res, const char *p, int *flags, int dowarn, int flag)
{
	long long l;
	char *ep;
	const char *vp;

	if (*p == '\0') {
		*flags |= flag;
		*res = 0;
		return 1;
	}
	l = strtoll(p, &ep, 0);
	if (p == ep || *ep != '\0') {
		vp = "Invalid number";
		goto done;
	}
	if (errno == ERANGE && (l == LLONG_MAX || l == LLONG_MIN)) {
		vp = strerror(errno);
		goto done;
	}
	_DIAGASSERT(__type_fit(time_t, l));
	*res = (time_t)l;
	return 1;
done:
	if (dowarn) {
		warnx("%s `%s' for %s time", vp, p,
		    flag == _PASSWORD_NOEXP ? "expiration" : "change");
	}
	return 0;

}

static int
getid(unsigned long *res, const char *p, int *flags, int dowarn, int flag)
{
	unsigned long ul;
	char *ep;

	if (*p == '\0') {
		*flags |= flag;
		*res = 0;
		return 1;
	}
	ul = strtoul(p, &ep, 0);
	if (p == ep || *ep != '\0') {
		ep = __UNCONST("Invalid number");
		goto done;
	}
	if (errno == ERANGE && ul == ULONG_MAX) {
		ep = strerror(errno);
		goto done;
	}
	if (ul > *res) {
		ep = strerror(ERANGE);
		goto done;
	}

	*res = ul;
	return 1;
done:
	if (dowarn)
		warnx("%s %s `%s'", ep, 
		    flag == _PASSWORD_NOUID ? "uid" : "gid", p);
	return 0;

}

int
#ifdef _LIBC
__pw_scan(char *bp, struct passwd *pw, int *flags)
#else
pw_scan( char *bp, struct passwd *pw, int *flags)
#endif
{
	unsigned long id;
	time_t ti;
	int root, inflags;
	int dowarn;
	const char *p, *sh;

	_DIAGASSERT(bp != NULL);
	_DIAGASSERT(pw != NULL);

	if (flags) {
		inflags = *flags;
		*flags = 0;
	} else {
		inflags = 0;
		flags = &inflags;
	}
	dowarn = !(inflags & _PASSWORD_NOWARN);

	if (!(pw->pw_name = strsep(&bp, ":")))		/* login */
		goto fmt;
	if (strlen(pw->pw_name) > (LOGIN_NAME_MAX - 1)) {
		if (dowarn)
			warnx("username too long, `%s' > %d", pw->pw_name,
			    LOGIN_NAME_MAX - 1);
		return 0;
	}

	root = !strcmp(pw->pw_name, "root");

	if (!(pw->pw_passwd = strsep(&bp, ":")))	/* passwd */
		goto fmt;

	if (!(p = strsep(&bp, ":")))			/* uid */
		goto fmt;

	id = UID_MAX;
	if (!getid(&id, p, flags, dowarn, _PASSWORD_NOUID))
		return 0;

	if (root && id) {
		if (dowarn)
			warnx("root uid should be 0");
		return 0;
	}

	pw->pw_uid = (uid_t)id;

	if (!(p = strsep(&bp, ":")))			/* gid */
		goto fmt;

	id = GID_MAX;
	if (!getid(&id, p, flags, dowarn, _PASSWORD_NOGID))
		return 0;

	pw->pw_gid = (gid_t)id;

	if (inflags & _PASSWORD_OLDFMT) {
		pw->pw_class = __UNCONST("");
		pw->pw_change = 0;
		pw->pw_expire = 0;
		*flags |= (_PASSWORD_NOCHG | _PASSWORD_NOEXP);
	} else {
		pw->pw_class = strsep(&bp, ":");	/* class */
		if (!(p = strsep(&bp, ":")))		/* change */
			goto fmt;
		if (!gettime(&ti, p, flags, dowarn, _PASSWORD_NOCHG))
			return 0;
		pw->pw_change = ti;

		if (!(p = strsep(&bp, ":")))		/* expire */
			goto fmt;
		if (!gettime(&ti, p, flags, dowarn, _PASSWORD_NOEXP))
			return 0;
		pw->pw_expire = ti;
	}

	pw->pw_gecos = strsep(&bp, ":");		/* gecos */
	pw->pw_dir = strsep(&bp, ":");			/* directory */
	if (!(pw->pw_shell = strsep(&bp, ":")))		/* shell */
		goto fmt;

#if ! HAVE_NBTOOL_CONFIG_H
	p = pw->pw_shell;
	if (root && *p)					/* empty == /bin/sh */
		for (setusershell();;) {
			if (!(sh = getusershell())) {
				if (dowarn)
					warnx("warning, unknown root shell");
				break;
			}
			if (!strcmp(p, sh))
				break;	
		}
#endif

	if ((p = strsep(&bp, ":")) != NULL) {			/* too many */
fmt:		
		if (dowarn)
			warnx("corrupted entry");
		return 0;
	}

	return 1;
}
