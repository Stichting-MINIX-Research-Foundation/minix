/*	$NetBSD: field.c,v 1.12 2009/04/11 12:10:02 lukem Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
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
#ifndef lint
#if 0
static char sccsid[] = "@(#)field.c	8.4 (Berkeley) 4/2/94";
#else 
__RCSID("$NetBSD: field.c,v 1.12 2009/04/11 12:10:02 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "chpass.h"
#include "pathnames.h"

/* ARGSUSED */
int
p_login(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!*p) {
		warnx("empty login field");
		return (1);
	}
	if (*p == '-') {
		warnx("login names may not begin with a hyphen");
		return (1);
	}
	if (!(pw->pw_name = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	if (strchr(p, '.'))
		warnx("\'.\' is dangerous in a login name");
	for (; *p; ++p)
		if (isupper((unsigned char)*p)) {
			warnx("upper-case letters are dangerous in a login name");
			break;
		}
	return (0);
}

/* ARGSUSED */
int
p_passwd(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!(pw->pw_passwd = strdup(p))) {
		warnx("can't save password entry");
		return (1);
	}
	
	return (0);
}

/* ARGSUSED */
int
p_uid(const char *p, struct passwd *pw, ENTRY *ep)
{
	unsigned long id;
	char *np;

	if (!*p) {
		warnx("empty uid field");
		return (1);
	}
	if (!isdigit((unsigned char)*p)) {
		warnx("illegal uid");
		return (1);
	}
	errno = 0;
	id = strtoul(p, &np, 10);
	/*
	 * We don't need to check the return value of strtoul()
	 * since ULONG_MAX is greater than UID_MAX.
	 */
	if (*np || id > UID_MAX) {
		warnx("illegal uid");
		return (1);
	}
	pw->pw_uid = (uid_t)id;
	return (0);
}

/* ARGSUSED */
int
p_gid(const char *p, struct passwd *pw, ENTRY *ep)
{
	struct group *gr;
	unsigned long id;
	char *np;

	if (!*p) {
		warnx("empty gid field");
		return (1);
	}
	if (!isdigit((unsigned char)*p)) {
		if (!(gr = getgrnam(p))) {
			warnx("unknown group %s", p);
			return (1);
		}
		pw->pw_gid = gr->gr_gid;
		return (0);
	}
	errno = 0;
	id = strtoul(p, &np, 10);
	/*
	 * We don't need to check the return value of strtoul() 
	 * since ULONG_MAX is greater than GID_MAX.
	 */
	if (*np || id > GID_MAX) {
		warnx("illegal gid");
		return (1);
	}
	pw->pw_gid = (gid_t)id;
	return (0);
}

/* ARGSUSED */
int
p_class(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!(pw->pw_class = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	
	return (0);
}

/* ARGSUSED */
int
p_change(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!atot(p, &pw->pw_change))
		return (0);
	warnx("illegal date for change field");
	return (1);
}

/* ARGSUSED */
int
p_expire(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!atot(p, &pw->pw_expire))
		return (0);
	warnx("illegal date for expire field");
	return (1);
}

/* ARGSUSED */
int
p_gecos(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!(ep->save = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_hdir(const char *p, struct passwd *pw, ENTRY *ep)
{

	if (!*p) {
		warnx("empty home directory field");
		return (1);
	}
	if (!(pw->pw_dir = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_shell(const char *p, struct passwd *pw, ENTRY *ep)
{
	const char *t;

	if (!*p) {
		if (!(pw->pw_shell = strdup(_PATH_BSHELL))) {
			warnx("can't save entry");
			return (1);
		}
		return (0);
	}
	/* only admin can change from or to "restricted" shells */
	if (uid && pw->pw_shell && !ok_shell(pw->pw_shell)) {
		warnx("%s: current shell non-standard", pw->pw_shell);
		return (1);
	}
	if (!(t = ok_shell(p))) {
		if (uid) {
			warnx("%s: non-standard shell", p);
			return (1);
		}
	}
	else
		p = t;
	if (!(pw->pw_shell = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}
