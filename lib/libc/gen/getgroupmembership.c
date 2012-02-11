/*	$NetBSD: getgroupmembership.c,v 1.4 2008/04/28 20:22:59 martin Exp $	*/

/*-
 * Copyright (c) 2004-2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
__RCSID("$NetBSD: getgroupmembership.c,v 1.4 2008/04/28 20:22:59 martin Exp $");
#endif /* LIBC_SCCS and not lint */

/*
 * calculate group access list
 */

#include "namespace.h"
#include "reentrant.h"

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <nsswitch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HESIOD
#include <hesiod.h>
#endif

#include "gr_private.h"

#ifdef __weak_alias
__weak_alias(getgroupmembership,_getgroupmembership)
#endif

/*
 * __gr_addgid
 *	Add gid to the groups array (of maxgrp size) at the position
 *	indicated by *groupc, unless it already exists or *groupc is
 *	past &groups[maxgrp].
 *	Returns 1 upon success (including duplicate suppression), 0 otherwise.
 */
static int
__gr_addgid(gid_t gid, gid_t *groups, int maxgrp, int *groupc)
{
	int	ret, dupc;

	_DIAGASSERT(groupc != NULL);
	_DIAGASSERT(groups != NULL);

						/* skip duplicates */
	for (dupc = 0; dupc < MIN(maxgrp, *groupc); dupc++) {
		if (groups[dupc] == gid)
			return 1;
	}

	ret = 1;
	if (*groupc < maxgrp)			/* add this gid */
		groups[*groupc] = gid;
	else
		ret = 0;
	(*groupc)++;
	return ret;
}


/*ARGSUSED*/
static int
_files_getgroupmembership(void *retval, void *cb_data, va_list ap)
{
	int		*result	= va_arg(ap, int *);
	const char 	*uname	= va_arg(ap, const char *);
	gid_t		 agroup	= va_arg(ap, gid_t);
	gid_t		*groups	= va_arg(ap, gid_t *);
	int		 maxgrp	= va_arg(ap, int);
	int		*groupc	= va_arg(ap, int *);

	struct __grstate_files	state;
	struct group		grp;
	char			grpbuf[_GETGR_R_SIZE_MAX];
	int			rv, i;

	_DIAGASSERT(result != NULL);
	_DIAGASSERT(uname != NULL);
	/* groups may be NULL if just sizing when invoked with maxgrp = 0 */
	_DIAGASSERT(groupc != NULL);

						/* install primary group */
	(void) __gr_addgid(agroup, groups, maxgrp, groupc);

	memset(&state, 0, sizeof(state));
	while (__grscan_files(&rv, &grp, grpbuf, sizeof(grpbuf), &state,
				0, NULL, 0) == NS_SUCCESS) {
						/* scan members */
		for (i = 0; grp.gr_mem[i]; i++) {
			if (strcmp(grp.gr_mem[i], uname) != 0)
				continue;
			if (! __gr_addgid(grp.gr_gid, groups, maxgrp, groupc))
				*result = -1;
			break;
		}
	}
	__grend_files(&state);
	return NS_NOTFOUND;
}


#ifdef HESIOD

/*ARGSUSED*/
static int
_dns_getgroupmembership(void *retval, void *cb_data, va_list ap)
{
	int		*result	= va_arg(ap, int *);
	const char 	*uname	= va_arg(ap, const char *);
	gid_t		 agroup	= va_arg(ap, gid_t);
	gid_t		*groups	= va_arg(ap, gid_t *);
	int		 maxgrp	= va_arg(ap, int);
	int		*groupc	= va_arg(ap, int *);

	struct __grstate_dns	state;
	struct group		grp;
	char			grpbuf[_GETGR_R_SIZE_MAX];
	unsigned long		id;
	void			*context;
	char			**hp, *cp, *ep;
	int			rv, i;

	_DIAGASSERT(result != NULL);
	_DIAGASSERT(uname != NULL);
	/* groups may be NULL if just sizing when invoked with maxgrp = 0 */
	_DIAGASSERT(groupc != NULL);

						/* install primary group */
	(void) __gr_addgid(agroup, groups, maxgrp, groupc);

	hp = NULL;
	rv = NS_NOTFOUND;

	if (hesiod_init(&context) == -1)		/* setup hesiod */
		return NS_UNAVAIL;

	hp = hesiod_resolve(context, uname, "grplist");	/* find grplist */
	if (hp == NULL) {
		if (errno != ENOENT) {			/* wasn't "not found"*/
			rv = NS_UNAVAIL;
			goto dnsgroupmembers_out;
		}
			/* grplist not found, fallback to _dns_grscan */
		memset(&state, 0, sizeof(state));
		while (__grscan_dns(&rv, &grp, grpbuf, sizeof(grpbuf), &state,
					0, NULL, 0) == NS_SUCCESS) {
							/* scan members */
			for (i = 0; grp.gr_mem[i]; i++) {
				if (strcmp(grp.gr_mem[i], uname) != 0)
					continue;
				if (! __gr_addgid(grp.gr_gid, groups, maxgrp,
				    groupc))
					*result = -1;
				break;
			}
		}
		__grend_dns(&state);
		rv = NS_NOTFOUND;
		goto dnsgroupmembers_out;
	}

	if ((ep = strchr(hp[0], '\n')) != NULL)
		*ep = '\0';				/* clear trailing \n */

	for (cp = hp[0]; *cp != '\0'; ) {		/* parse grplist */
		if ((cp = strchr(cp, ':')) == NULL)	/* skip grpname */
			break;
		cp++;
		id = strtoul(cp, &ep, 10);		/* parse gid */
		if (id > GID_MAX || (*ep != ':' && *ep != '\0')) {
			rv = NS_UNAVAIL;
			goto dnsgroupmembers_out;
		}
		cp = ep;
		if (*cp == ':')
			cp++;

							/* add gid */
		if (! __gr_addgid((gid_t)id, groups, maxgrp, groupc))
			*result = -1;
	}

	rv = NS_NOTFOUND;

 dnsgroupmembers_out:
	if (hp)
		hesiod_free_list(context, hp);
	hesiod_end(context);
	return rv;
}

#endif /* HESIOD */


#ifdef YP

/*ARGSUSED*/
static int
_nis_getgroupmembership(void *retval, void *cb_data, va_list ap)
{
	int		*result	= va_arg(ap, int *);
	const char 	*uname	= va_arg(ap, const char *);
	gid_t		 agroup	= va_arg(ap, gid_t);
	gid_t		*groups	= va_arg(ap, gid_t *);
	int		 maxgrp	= va_arg(ap, int);
	int		*groupc	= va_arg(ap, int *);

	struct __grstate_nis	state;
	struct group		grp;
	char			grpbuf[_GETGR_R_SIZE_MAX];
	int			rv, i;

	_DIAGASSERT(result != NULL);
	_DIAGASSERT(uname != NULL);
	/* groups may be NULL if just sizing when invoked with maxgrp = 0 */
	_DIAGASSERT(groupc != NULL);

						/* install primary group */
	(void) __gr_addgid(agroup, groups, maxgrp, groupc);

	memset(&state, 0, sizeof(state));
	while (__grscan_nis(&rv, &grp, grpbuf, sizeof(grpbuf), &state,
				0, NULL, 0) == NS_SUCCESS) {
						/* scan members */
		for (i = 0; grp.gr_mem[i]; i++) {
			if (strcmp(grp.gr_mem[i], uname) != 0)
				continue;
			if (! __gr_addgid(grp.gr_gid, groups, maxgrp, groupc))
				*result = -1;
			break;
		}
	}
	__grend_nis(&state);

	return NS_NOTFOUND;
}

#endif /* YP */


#ifdef _GROUP_COMPAT

struct __compatggm {
	const char	*uname;		/* user to search for */
	gid_t		*groups;
	gid_t		 agroup;
	int		 maxgrp;
	int		*groupc;
};

static int
_compat_ggm_search(void *cookie, struct group **groupres)
{
	struct __compatggm	*cp;
	int			rerror, crv;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_getgroupmembership, NULL)
		NS_NIS_CB(_nis_getgroupmembership, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};

	*groupres = NULL;	/* we don't care about this */
	cp = (struct __compatggm *)cookie;

	crv = nsdispatch(NULL, dtab,
	    NSDB_GROUP_COMPAT, "getgroupmembership",
	    __nsdefaultnis,
	    &rerror, cp->uname, cp->agroup, cp->groups, cp->maxgrp, cp->groupc);

	if (crv == NS_SUCCESS)
		crv = NS_NOTFOUND;	/* indicate "no more +: entries" */

	return crv;
}

/* ARGSUSED */
static int
_compat_getgroupmembership(void *retval, void *cb_data, va_list ap)
{
	int		*result	= va_arg(ap, int *);
	const char 	*uname	= va_arg(ap, const char *);
	gid_t		 agroup	= va_arg(ap, gid_t);
	gid_t		*groups	= va_arg(ap, gid_t *);
	int		 maxgrp	= va_arg(ap, int);
	int		*groupc	= va_arg(ap, int *);

	struct __grstate_compat	state;
	struct __compatggm	ggmstate;
	struct group		grp;
	char			grpbuf[_GETGR_R_SIZE_MAX];
	int			rv, i;

	_DIAGASSERT(result != NULL);
	_DIAGASSERT(uname != NULL);
	/* groups may be NULL if just sizing when invoked with maxgrp = 0 */
	_DIAGASSERT(groupc != NULL);

						/* install primary group */
	(void) __gr_addgid(agroup, groups, maxgrp, groupc);

	memset(&state, 0, sizeof(state));
	memset(&ggmstate, 0, sizeof(ggmstate));
	ggmstate.uname = uname;
	ggmstate.groups = groups;
	ggmstate.agroup = agroup;
	ggmstate.maxgrp = maxgrp;
	ggmstate.groupc = groupc;

	while (__grscan_compat(&rv, &grp, grpbuf, sizeof(grpbuf), &state,
				0, NULL, 0, _compat_ggm_search, &ggmstate)
		== NS_SUCCESS) {
						/* scan members */
		for (i = 0; grp.gr_mem[i]; i++) {
			if (strcmp(grp.gr_mem[i], uname) != 0)
				continue;
			if (! __gr_addgid(grp.gr_gid, groups, maxgrp, groupc))
				*result = -1;
			break;
		}
	}

	__grend_compat(&state);
	return NS_NOTFOUND;
}

#endif	/* _GROUP_COMPAT */


int
getgroupmembership(const char *uname, gid_t agroup,
    gid_t *groups, int maxgrp, int *groupc)
{
	int	rerror;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgroupmembership, NULL)
		NS_DNS_CB(_dns_getgroupmembership, NULL)
		NS_NIS_CB(_nis_getgroupmembership, NULL)
		NS_COMPAT_CB(_compat_getgroupmembership, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(uname != NULL);
	/* groups may be NULL if just sizing when invoked with maxgrp = 0 */
	_DIAGASSERT(groupc != NULL);

	*groupc = 0;

	mutex_lock(&__grmutex);
			/*
			 * Call each backend.
			 * For compatibility with getgrent(3) semantics,
			 * a backend should return NS_NOTFOUND even upon
			 * completion, to allow result merging to occur.
			 */
	(void) nsdispatch(NULL, dtab, NSDB_GROUP, "getgroupmembership",
	    __nsdefaultcompat,
	    &rerror, uname, agroup, groups, maxgrp, groupc);
	mutex_unlock(&__grmutex);

	if (*groupc > maxgrp)			/* too many groups found */
		return -1;
	else
		return 0;
}
