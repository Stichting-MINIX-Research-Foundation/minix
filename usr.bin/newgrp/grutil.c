/*	$NetBSD: grutil.c,v 1.4 2014/06/23 06:57:31 shm Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brian Ginsbach.
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
__RCSID("$NetBSD: grutil.c,v 1.4 2014/06/23 06:57:31 shm Exp $");

#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#ifdef LOGIN_CAP
#include <login_cap.h>
#endif

#include "grutil.h"

typedef enum {
	ADDGRP_NOERROR		= 0,	/* must be zero */
	ADDGRP_EMALLOC		= 1,
	ADDGRP_EGETGROUPS	= 2,
	ADDGRP_ESETGROUPS	= 3
} addgrp_ret_t;

static void
free_groups(void *groups)
{
	int oerrno;

	oerrno = errno;
	free(groups);
	errno = oerrno;
}

static addgrp_ret_t
alloc_groups(int *ngroups, gid_t **groups, int *ngroupsmax)
{
	*ngroupsmax = (int)sysconf(_SC_NGROUPS_MAX);
	if (*ngroupsmax < 0)
		*ngroupsmax = NGROUPS_MAX;

	*groups = malloc(*ngroupsmax * sizeof(**groups));
	if (*groups == NULL)
		return ADDGRP_EMALLOC;

	*ngroups = getgroups(*ngroupsmax, *groups);
	if (*ngroups == -1) {
		free_groups(*groups);
		return ADDGRP_ESETGROUPS;
	}
	return ADDGRP_NOERROR;
}

static addgrp_ret_t
addgid(gid_t *groups, int ngroups, int ngroupsmax, gid_t gid, int makespace)
{
	int i;

	/* search for gid in supplemental group list */
	for (i = 0; i < ngroups && groups[i] != gid; i++)
		continue;

	/* add the gid to the supplemental group list */
	if (i == ngroups) {
		if (ngroups < ngroupsmax)
			groups[ngroups++] = gid;
		else {	/*
			 * setgroups(2) will fail with errno = EINVAL
			 * if ngroups > nmaxgroups.  If makespace is
			 * set, replace the last group with the new
			 * one.  Otherwise, fail the way setgroups(2)
			 * would if we passed the larger groups array.
			 */
			if (makespace) {
				/*
				 * Find a slot that doesn't contain
				 * the primary group.
				 */
				struct passwd *pwd;
				gid_t pgid;
				pwd = getpwuid(getuid());
				if (pwd == NULL)
					goto error;
				pgid = pwd->pw_gid;
				for (i = ngroupsmax - 1; i >= 0; i--)
					if (groups[i] != pgid)
						break;
				if (i < 0)
					goto error;
				groups[i] = gid;
			}
			else {
		error:
				errno = EINVAL;
				return ADDGRP_ESETGROUPS;
			}
		}
		if (setgroups(ngroups, groups) < 0)
			return ADDGRP_ESETGROUPS;
	}
	return ADDGRP_NOERROR;
}

static addgrp_ret_t
addgrp(gid_t newgid, int makespace)
{
	int ngroups, ngroupsmax;
	addgrp_ret_t rval;
	gid_t *groups;
	gid_t oldgid;

	oldgid = getgid();
	if (oldgid == newgid) /* nothing to do */
		return ADDGRP_NOERROR;

	rval = alloc_groups(&ngroups, &groups, &ngroupsmax);
	if (rval != ADDGRP_NOERROR)
		return rval;

	/*
	 * BSD based systems normally have the egid in the supplemental
	 * group list.
	 */
#if (defined(BSD) && BSD >= 199306)
	/*
	 * According to POSIX/XPG6:
	 * On system where the egid is normally in the supplemental group list
	 * (or whenever the old egid actually is in the supplemental group
	 * list):
	 *	o If the new egid is in the supplemental group list,
	 *	  just change the egid.
	 *	o If the new egid is not in the supplemental group list,
	 *	  add the new egid to the list if there is room.
	 */

	rval = addgid(groups, ngroups, ngroupsmax, newgid, makespace);
#else
	/*
	 * According to POSIX/XPG6:
	 * On systems where the egid is not normally in the supplemental group
	 * list (or whenever the old egid is not in the supplemental group
	 * list):
	 *	o If the new egid is in the supplemental group list, delete
	 *	  it from the list.
	 *	o If the old egid is not in the supplemental group list,
	 *	  add the old egid to the list if there is room.
	 */
	{
		int i;

		/* search for new egid in supplemental group list */
		for (i = 0; i < ngroups && groups[i] != newgid; i++)
			continue;

		/* remove new egid from supplemental group list */
		if (i != ngroups)
			for (--ngroups; i < ngroups; i++)
				groups[i] = groups[i + 1];

		rval = addgid(groups, ngroups, ngroupsmax, oldgid, makespace);
	}
#endif
	free_groups(groups);
	return rval;
}

/*
 * If newgrp fails, it returns (gid_t)-1 and the errno variable is
 * set to:
 *	[EINVAL]	Unknown group.
 *	[EPERM]		Bad password.
 */
static gid_t
newgrp(const char *gname, struct passwd *pwd, uid_t ruid, const char *prompt)
{
	struct group *grp;
	char **ap;
	char *p;
	gid_t *groups;
	int ngroups, ngroupsmax;

	if (gname == NULL)
		return pwd->pw_gid;

	grp = getgrnam(gname);

#ifdef GRUTIL_ACCEPT_GROUP_NUMBERS
	if (grp == NULL) {
		gid_t gid;
		if (*gname != '-') {
		    gid = (gid_t)strtol(gname, &p, 10);
		    if (*p == '\0')
			    grp = getgrgid(gid);
		}
	}
#endif
	if (grp == NULL) {
		errno = EINVAL;
		return (gid_t)-1;
	}

	if (ruid == 0 || pwd->pw_gid == grp->gr_gid)
		return grp->gr_gid;

	if (alloc_groups(&ngroups, &groups, &ngroupsmax) == ADDGRP_NOERROR) {
		int i;
		for (i = 0; i < ngroups; i++)
			if (groups[i] == grp->gr_gid) {
				free_groups(groups);
				return grp->gr_gid;
			}
		free_groups(groups);
	}

	/*
	 * Check the group membership list in case the groups[] array
	 * was maxed out or the user has been added to it since login.
	 */
	for (ap = grp->gr_mem; *ap != NULL; ap++)
		if (strcmp(*ap, pwd->pw_name) == 0)
			return grp->gr_gid;

	if (*grp->gr_passwd != '\0') {
		p = getpass(prompt);
		if (strcmp(grp->gr_passwd, crypt(p, grp->gr_passwd)) == 0) {
			(void)memset(p, '\0', _PASSWORD_LEN);
			return grp->gr_gid;
		}
		(void)memset(p, '\0', _PASSWORD_LEN);
	}

	errno = EPERM;
	return (gid_t)-1;
}

#ifdef GRUTIL_SETGROUPS_MAKESPACE
# define ADDGRP_MAKESPACE	1
#else
# define ADDGRP_MAKESPACE	0
#endif

#ifdef GRUTIL_ALLOW_GROUP_ERRORS
# define maybe_exit(e)
#else
# define maybe_exit(e)	exit(e);
#endif

void
addgroup(
#ifdef LOGIN_CAP
    login_cap_t *lc,
#endif
    const char *gname, struct passwd *pwd, uid_t ruid, const char *prompt)
{
	pwd->pw_gid = newgrp(gname, pwd, ruid, prompt);
	if (pwd->pw_gid == (gid_t)-1) {
		switch (errno) {
		case EINVAL:
			warnx("Unknown group `%s'", gname);
			maybe_exit(EXIT_FAILURE);
			break;
		case EPERM:	/* password failure */
			warnx("Sorry");
			maybe_exit(EXIT_FAILURE);
			break;
		default: /* XXX - should never happen */
			err(EXIT_FAILURE, "unknown error");
			break;
		}
		pwd->pw_gid = getgid();
	}

	switch (addgrp(pwd->pw_gid, ADDGRP_MAKESPACE)) {
	case ADDGRP_NOERROR:
		break;
	case ADDGRP_EMALLOC:
		err(EXIT_FAILURE, "malloc");
		break;
	case ADDGRP_EGETGROUPS:
		err(EXIT_FAILURE, "getgroups");
		break;
	case ADDGRP_ESETGROUPS:
		switch(errno) {
		case EINVAL:
			warnx("setgroups: ngroups > ngroupsmax");
			maybe_exit(EXIT_FAILURE);
			break;
		case EPERM:
		case EFAULT:
		default:
			warn("setgroups");
			maybe_exit(EXIT_FAILURE);
			break;
		}
		break;
	}

#ifdef LOGIN_CAP
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGID) == -1)
		err(EXIT_FAILURE, "setting user context");
#else
	if (setgid(pwd->pw_gid) == -1)
		err(EXIT_FAILURE, "setgid");
#endif
}
