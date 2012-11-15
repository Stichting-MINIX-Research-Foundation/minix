/*	$NetBSD: getgrent.c,v 1.67 2012/08/29 18:50:35 dholland Exp $	*/

/*-
 * Copyright (c) 1999-2000, 2004-2005 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1989, 1993
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

/*
 * Portions Copyright (c) 1994, Jason Downs. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)getgrent.c	8.2 (Berkeley) 3/21/94";
#else
__RCSID("$NetBSD: getgrent.c,v 1.67 2012/08/29 18:50:35 dholland Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

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
#include <syslog.h>

#ifdef HESIOD
#include <hesiod.h>
#endif

#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#include "gr_private.h"

#ifdef __weak_alias
__weak_alias(endgrent,_endgrent)
__weak_alias(getgrent,_getgrent)
__weak_alias(getgrent_r,_getgrent_r)
__weak_alias(getgrgid,_getgrgid)
__weak_alias(getgrgid_r,_getgrgid_r)
__weak_alias(getgrnam,_getgrnam)
__weak_alias(getgrnam_r,_getgrnam_r)
__weak_alias(setgrent,_setgrent)
__weak_alias(setgroupent,_setgroupent)
#endif

#ifdef _REENTRANT
mutex_t	__grmutex = MUTEX_INITIALIZER;
#endif

/*
 * _gr_memfrombuf
 *	Obtain want bytes from buffer (of size buflen) and return a pointer
 *	to the available memory after adjusting buffer/buflen.
 *	Returns NULL if there is insufficient space.
 */
static char *
_gr_memfrombuf(size_t want, char **buffer, size_t *buflen)
{
	char	*rv;

	if (want > *buflen) {
		errno = ERANGE;
		return NULL;
	}
	rv = *buffer;
	*buffer += want;
	*buflen -= want;
	return rv;
}

/*
 * _gr_parse
 *	Parses entry as a line per group(5) (without the trailing \n)
 *	and fills in grp with corresponding values; memory for strings
 *	and arrays will be allocated from buf (of size buflen).
 *	Returns 1 if parsed successfully, 0 on parse failure.
 */
static int
_gr_parse(const char *entry, struct group *grp, char *buf, size_t buflen)
{
	unsigned long	id;
	const char	*bp;
	char		*ep;
	size_t		count;
	int		memc;

	_DIAGASSERT(entry != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buf != NULL);

#define COPYTOBUF(to) \
	do { \
		(to) = _gr_memfrombuf(count+1, &buf, &buflen); \
		if ((to) == NULL) \
			return 0; \
		memmove((to), entry, count); \
		to[count] = '\0'; \
	} while (0)	/* LINTED */

#if 0
	if (*entry == '+')			/* fail on compat `+' token */
		return 0;
#endif

	count = strcspn(entry, ":");		/* parse gr_name */
	if (entry[count] == '\0')
		return 0;
	COPYTOBUF(grp->gr_name);
	entry += count + 1;

	count = strcspn(entry, ":");		/* parse gr_passwd */
	if (entry[count] == '\0')
		return 0;
	COPYTOBUF(grp->gr_passwd);
	entry += count + 1;

	count = strcspn(entry, ":");		/* parse gr_gid */
	if (entry[count] == '\0')
		return 0;
	id = strtoul(entry, &ep, 10);
	if (id > GID_MAX || *ep != ':')
		return 0;
	grp->gr_gid = (gid_t)id;
	entry += count + 1;

	memc = 1;				/* for final NULL */
	if (*entry != '\0')
		memc++;				/* for first item */
	for (bp = entry; *bp != '\0'; bp++) {
		if (*bp == ',')
			memc++;
	}
				/* grab ALIGNed char **gr_mem from buf */
	ep = _gr_memfrombuf(memc * sizeof(char *) + ALIGNBYTES, &buf, &buflen);
	if (ep == NULL)
		return 0;
	grp->gr_mem = (char **)ALIGN(ep);

	for (memc = 0; *entry != '\0'; memc++) {
		count = strcspn(entry, ",");	/* parse member */
		COPYTOBUF(grp->gr_mem[memc]);
		entry += count;
		if (*entry == ',')
			entry++;
	}

#undef COPYTOBUF

	grp->gr_mem[memc] = NULL;
	return 1;
}

/*
 * _gr_copy
 *	Copy the contents of fromgrp to grp; memory for strings
 *	and arrays will be allocated from buf (of size buflen).
 *	Returns 1 if copied successfully, 0 on copy failure.
 *	NOTE: fromgrp must not use buf for its own pointers.
 */
static int
_gr_copy(struct group *fromgrp, struct group *grp, char *buf, size_t buflen)
{
	char	*ep;
	int	memc;

	_DIAGASSERT(fromgrp != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buf != NULL);

#define COPYSTR(to, from) \
	do { \
		size_t count = strlen((from)); \
		(to) = _gr_memfrombuf(count+1, &buf, &buflen); \
		if ((to) == NULL) \
			return 0; \
		memmove((to), (from), count); \
		to[count] = '\0'; \
	} while (0)	/* LINTED */

	COPYSTR(grp->gr_name, fromgrp->gr_name);
	COPYSTR(grp->gr_passwd, fromgrp->gr_passwd);
	grp->gr_gid = fromgrp->gr_gid;

	if (fromgrp->gr_mem == NULL)
		return 0;

	for (memc = 0; fromgrp->gr_mem[memc]; memc++)
		continue;
	memc++;					/* for final NULL */

				/* grab ALIGNed char **gr_mem from buf */
	ep = _gr_memfrombuf(memc * sizeof(char *) + ALIGNBYTES, &buf, &buflen);
	grp->gr_mem = (char **)ALIGN(ep);
	if (grp->gr_mem == NULL)
		return 0;

	for (memc = 0; fromgrp->gr_mem[memc]; memc++) {
		COPYSTR(grp->gr_mem[memc], fromgrp->gr_mem[memc]);
	}

#undef COPYSTR

	grp->gr_mem[memc] = NULL;
	return 1;
}

		/*
		 *	files methods
		 */

int
__grstart_files(struct __grstate_files *state)
{

	_DIAGASSERT(state != NULL);

	if (state->fp == NULL) {
		state->fp = fopen(_PATH_GROUP, "re");
		if (state->fp == NULL)
			return NS_UNAVAIL;
	} else {
		rewind(state->fp);
	}
	return NS_SUCCESS;
}

int
__grend_files(struct __grstate_files *state)
{

	_DIAGASSERT(state != NULL);

	if (state->fp) {
		(void) fclose(state->fp);
		state->fp = NULL;
	}
	return NS_SUCCESS;
}

/*
 * __grscan_files
 *	Scan state->fp for the next desired entry.
 *	If search is zero, return the next entry.
 *	If search is non-zero, look for a specific name (if name != NULL),
 *	or a specific gid (if name == NULL).
 *	Sets *retval to the errno if the result is not NS_SUCCESS
 *	or NS_NOTFOUND.
 */
int
__grscan_files(int *retval, struct group *grp, char *buffer, size_t buflen,
	struct __grstate_files *state, int search, const char *name, gid_t gid)
{
	int	rv;
	char	filebuf[_GETGR_R_SIZE_MAX], *ep;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	/* name is NULL to indicate searching for gid */

	*retval = 0;

	if (state->fp == NULL) {	/* only start if file not open yet */
		rv = __grstart_files(state);
		if (rv != NS_SUCCESS)
			goto filesgrscan_out;
	}

	rv = NS_NOTFOUND;

							/* scan line by line */
	while (fgets(filebuf, (int)sizeof(filebuf), state->fp) != NULL) {
		ep = strchr(filebuf, '\n');
		if (ep == NULL) {	/* skip lines that are too big */
			int ch;

			while ((ch = getc(state->fp)) != '\n' && ch != EOF)
				continue;
			continue;
		}
		*ep = '\0';				/* clear trailing \n */

		if (filebuf[0] == '+')			/* skip compat line */
			continue;

							/* validate line */
		if (! _gr_parse(filebuf, grp, buffer, buflen)) {
			continue;			/* skip bad lines */
		}
		if (! search) {				/* just want this one */
			rv = NS_SUCCESS;
			break;
		}
							/* want specific */
		if ((name && strcmp(name, grp->gr_name) == 0) ||
		    (!name && gid == grp->gr_gid)) {
			rv = NS_SUCCESS;
			break;
		}
	}

 filesgrscan_out:
	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	return rv;
}


static struct __grstate_files	_files_state;
					/* storage for non _r functions */
static struct group		_files_group;
static char			_files_groupbuf[_GETGR_R_SIZE_MAX];

/*ARGSUSED*/
static int
_files_setgrent(void *nsrv, void *nscb, va_list ap)
{

	_files_state.stayopen = 0;
	return __grstart_files(&_files_state);
}

/*ARGSUSED*/
static int
_files_setgroupent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	_files_state.stayopen = stayopen;
	rv = __grstart_files(&_files_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_files_endgrent(void *nsrv, void *nscb, va_list ap)
{

	_files_state.stayopen = 0;
	return __grend_files(&_files_state);
}

/*ARGSUSED*/
static int
_files_getgrent(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grscan_files(&rerror, &_files_group,
	    _files_groupbuf, sizeof(_files_groupbuf),
	    &_files_state, 0, NULL, 0);
	if (rv == NS_SUCCESS)
		*retval = &_files_group;
	return rv;
}

/*ARGSUSED*/
static int
_files_getgrent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	rv = __grscan_files(retval, grp, buffer, buflen,
	    &_files_state, 0, NULL, 0);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*result = NULL;
	return rv;
}

/*ARGSUSED*/
static int
_files_getgrgid(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	gid_t		 gid	= va_arg(ap, gid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_files(&_files_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_files(&rerror, &_files_group,
	    _files_groupbuf, sizeof(_files_groupbuf),
	    &_files_state, 1, NULL, gid);
	if (!_files_state.stayopen)
		__grend_files(&_files_state);
	if (rv == NS_SUCCESS)
		*retval = &_files_group;
	return rv;
}

/*ARGSUSED*/
static int
_files_getgrgid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	gid_t		 gid	= va_arg(ap, gid_t);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_files state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = __grscan_files(retval, grp, buffer, buflen, &state, 1, NULL, gid);
	__grend_files(&state);
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

/*ARGSUSED*/
static int
_files_getgrnam(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_files(&_files_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_files(&rerror, &_files_group,
	    _files_groupbuf, sizeof(_files_groupbuf),
	    &_files_state, 1, name, 0);
	if (!_files_state.stayopen)
		__grend_files(&_files_state);
	if (rv == NS_SUCCESS)
		*retval = &_files_group;
	return rv;
}

/*ARGSUSED*/
static int
_files_getgrnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_files state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = __grscan_files(retval, grp, buffer, buflen, &state, 1, name, 0);
	__grend_files(&state);
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}


#ifdef HESIOD
		/*
		 *	dns methods
		 */

int
__grstart_dns(struct __grstate_dns *state)
{

	_DIAGASSERT(state != NULL);

	state->num = 0;
	if (state->context == NULL) {			/* setup Hesiod */
		if (hesiod_init(&state->context) == -1)
			return NS_UNAVAIL;
	}

	return NS_SUCCESS;
}

int
__grend_dns(struct __grstate_dns *state)
{

	_DIAGASSERT(state != NULL);

	state->num = 0;
	if (state->context) {
		hesiod_end(state->context);
		state->context = NULL;
	}
	return NS_SUCCESS;
}

/*
 * __grscan_dns
 *	Search Hesiod for the next desired entry.
 *	If search is zero, return the next entry.
 *	If search is non-zero, look for a specific name (if name != NULL),
 *	or a specific gid (if name == NULL).
 */
int
__grscan_dns(int *retval, struct group *grp, char *buffer, size_t buflen,
	struct __grstate_dns *state, int search, const char *name, gid_t gid)
{
	const char	**curzone;
	char		**hp, *ep;
	int		rv;

	static const char *zones_gid_group[] = {
		"gid",
		"group",
		NULL
	};

	static const char *zones_group[] = {
		"group",
		NULL
	};

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	/* name is NULL to indicate searching for gid */

	*retval = 0;

	if (state->context == NULL) {	/* only start if Hesiod not setup */
		rv = __grstart_dns(state);
		if (rv != NS_SUCCESS)
			return rv;
	}

 next_dns_entry:
	hp = NULL;
	rv = NS_NOTFOUND;

	if (! search) {			/* find next entry */
		if (state->num == -1)		/* exhausted search */
			return NS_NOTFOUND;
						/* find group-NNN */
		snprintf(buffer, buflen, "group-%u", state->num);
		state->num++;
		curzone = zones_group;
	} else if (name) {		/* find group name */
		snprintf(buffer, buflen, "%s", name);
		curzone = zones_group;
	} else {			/* find gid */
		snprintf(buffer, buflen, "%u", (unsigned int)gid);
		curzone = zones_gid_group;
	}

	for (; *curzone; curzone++) {		/* search zones */
		hp = hesiod_resolve(state->context, buffer, *curzone);
		if (hp != NULL)
			break;
		if (errno != ENOENT) {
			rv = NS_UNAVAIL;
			goto dnsgrscan_out;
		}
	}
	if (*curzone == NULL) {
		if (! search)
			state->num = -1;
		goto dnsgrscan_out;
	}

	if ((ep = strchr(hp[0], '\n')) != NULL)
		*ep = '\0';				/* clear trailing \n */
	if (_gr_parse(hp[0], grp, buffer, buflen)) {	/* validate line */
		if (! search) {				/* just want this one */
			rv = NS_SUCCESS;
		} else if ((name && strcmp(name, grp->gr_name) == 0) ||
		    (!name && gid == grp->gr_gid)) {	/* want specific */
			rv = NS_SUCCESS;
		}
	} else {					/* dodgy entry */
		if (!search) {			/* try again if ! searching */
			hesiod_free_list(state->context, hp);
			goto next_dns_entry;
		}
	}

 dnsgrscan_out:
	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	if (hp)
		hesiod_free_list(state->context, hp);
	return rv;
}

static struct __grstate_dns	_dns_state;
					/* storage for non _r functions */
static struct group		_dns_group;
static char			_dns_groupbuf[_GETGR_R_SIZE_MAX];

/*ARGSUSED*/
static int
_dns_setgrent(void *nsrv, void *nscb, va_list ap)
{

	_dns_state.stayopen = 0;
	return __grstart_dns(&_dns_state);
}

/*ARGSUSED*/
static int
_dns_setgroupent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	_dns_state.stayopen = stayopen;
	rv = __grstart_dns(&_dns_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_dns_endgrent(void *nsrv, void *nscb, va_list ap)
{

	_dns_state.stayopen = 0;
	return __grend_dns(&_dns_state);
}

/*ARGSUSED*/
static int
_dns_getgrent(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);

	int	  rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grscan_dns(&rerror, &_dns_group,
	    _dns_groupbuf, sizeof(_dns_groupbuf), &_dns_state, 0, NULL, 0);
	if (rv == NS_SUCCESS)
		*retval = &_dns_group;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getgrent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	rv = __grscan_dns(retval, grp, buffer, buflen,
	    &_dns_state, 0, NULL, 0);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*result = NULL;
	return rv;
}
/*ARGSUSED*/
static int
_dns_getgrgid(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	gid_t		 gid	= va_arg(ap, gid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_dns(&_dns_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_dns(&rerror, &_dns_group,
	    _dns_groupbuf, sizeof(_dns_groupbuf), &_dns_state, 1, NULL, gid);
	if (!_dns_state.stayopen)
		__grend_dns(&_dns_state);
	if (rv == NS_SUCCESS)
		*retval = &_dns_group;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getgrgid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	gid_t		 gid	= va_arg(ap, gid_t);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_dns state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = __grscan_dns(retval, grp, buffer, buflen, &state, 1, NULL, gid);
	__grend_dns(&state);
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getgrnam(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_dns(&_dns_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_dns(&rerror, &_dns_group,
	    _dns_groupbuf, sizeof(_dns_groupbuf), &_dns_state, 1, name, 0);
	if (!_dns_state.stayopen)
		__grend_dns(&_dns_state);
	if (rv == NS_SUCCESS)
		*retval = &_dns_group;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getgrnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_dns state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = __grscan_dns(retval, grp, buffer, buflen, &state, 1, name, 0);
	__grend_dns(&state);
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

#endif /* HESIOD */


#ifdef YP
		/*
		 *	nis methods
		 */

int
__grstart_nis(struct __grstate_nis *state)
{

	_DIAGASSERT(state != NULL);

	state->done = 0;
	if (state->current) {
		free(state->current);
		state->current = NULL;
	}
	if (state->domain == NULL) {			/* setup NIS */
		switch (yp_get_default_domain(&state->domain)) {
		case 0:
			break;
		case YPERR_RESRC:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	}
	return NS_SUCCESS;
}

int
__grend_nis(struct __grstate_nis *state)
{

	_DIAGASSERT(state != NULL);

	if (state->domain) {
		state->domain = NULL;
	}
	state->done = 0;
	if (state->current) {
		free(state->current);
		state->current = NULL;
	}
	return NS_SUCCESS;
}

/*
 * __grscan_nis
 *	Search NIS for the next desired entry.
 *	If search is zero, return the next entry.
 *	If search is non-zero, look for a specific name (if name != NULL),
 *	or a specific gid (if name == NULL).
 */
int
__grscan_nis(int *retval, struct group *grp, char *buffer, size_t buflen,
	struct __grstate_nis *state, int search, const char *name, gid_t gid)
{
	const char *map;
	char	*key, *data;
	int	nisr, rv, keylen, datalen;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	/* name is NULL to indicate searching for gid */

	*retval = 0;

	if (state->domain == NULL) {	/* only start if NIS not setup */
		rv = __grstart_nis(state);
		if (rv != NS_SUCCESS)
			return rv;
	}

 next_nis_entry:
	key = NULL;
	data = NULL;
	rv = NS_SUCCESS;

	if (! search) 	{			/* find next entry */
		if (state->done)			/* exhausted search */
			return NS_NOTFOUND;
		map = "group.byname";
		if (state->current) {			/* already searching */
			nisr = yp_next(state->domain, map,
			    state->current, state->currentlen,
			    &key, &keylen, &data, &datalen);
			free(state->current);
			state->current = NULL;
			switch (nisr) {
			case 0:
				state->current = key;
				state->currentlen = keylen;
				key = NULL;
				break;
			case YPERR_NOMORE:
				rv = NS_NOTFOUND;
				state->done = 1;
				break;
			default:
				rv = NS_UNAVAIL;
				break;
			}
		} else {				/* new search */
			if (yp_first(state->domain, map,
			    &state->current, &state->currentlen,
			    &data, &datalen)) {
				rv = NS_UNAVAIL;
			}
		}
	} else {				/* search for specific item */
		if (name) {			/* find group name */
			snprintf(buffer, buflen, "%s", name);
			map = "group.byname";
		} else {			/* find gid */
			snprintf(buffer, buflen, "%u", (unsigned int)gid);
			map = "group.bygid";
		}
		nisr = yp_match(state->domain, map, buffer, (int)strlen(buffer),
		    &data, &datalen);
		switch (nisr) {
		case 0:
			break;
		case YPERR_KEY:
			rv = NS_NOTFOUND;
			break;
		default:
			rv = NS_UNAVAIL;
			break;
		}
	}
	if (rv == NS_SUCCESS) {				/* validate data */
		data[datalen] = '\0';			/* clear trailing \n */
		if (_gr_parse(data, grp, buffer, buflen)) {
			if (! search) {			/* just want this one */
				rv = NS_SUCCESS;
			} else if ((name && strcmp(name, grp->gr_name) == 0) ||
			    (!name && gid == grp->gr_gid)) {
							/* want specific */
				rv = NS_SUCCESS;
			}
		} else {				/* dodgy entry */
			if (!search) {		/* try again if ! searching */
				free(data);
				goto next_nis_entry;
			}
		}
	}

	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	if (key)
		free(key);
	if (data)
		free(data);
	return rv;
}

static struct __grstate_nis	_nis_state;
					/* storage for non _r functions */
static struct group		_nis_group;
static char			_nis_groupbuf[_GETGR_R_SIZE_MAX];

/*ARGSUSED*/
static int
_nis_setgrent(void *nsrv, void *nscb, va_list ap)
{

	_nis_state.stayopen = 0;
	return __grstart_nis(&_nis_state);
}

/*ARGSUSED*/
static int
_nis_setgroupent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	_nis_state.stayopen = stayopen;
	rv = __grstart_nis(&_nis_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_nis_endgrent(void *nsrv, void *nscb, va_list ap)
{

	return __grend_nis(&_nis_state);
}

/*ARGSUSED*/
static int
_nis_getgrent(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grscan_nis(&rerror, &_nis_group,
	    _nis_groupbuf, sizeof(_nis_groupbuf), &_nis_state, 0, NULL, 0);
	if (rv == NS_SUCCESS)
		*retval = &_nis_group;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getgrent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	rv = __grscan_nis(retval, grp, buffer, buflen,
	    &_nis_state, 0, NULL, 0);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*result = NULL;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getgrgid(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	gid_t		 gid	= va_arg(ap, gid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_nis(&_nis_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_nis(&rerror, &_nis_group,
	    _nis_groupbuf, sizeof(_nis_groupbuf), &_nis_state, 1, NULL, gid);
	if (!_nis_state.stayopen)
		__grend_nis(&_nis_state);
	if (rv == NS_SUCCESS)
		*retval = &_nis_group;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getgrgid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	gid_t		 gid	= va_arg(ap, gid_t);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_nis state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
/* remark: we run under a global mutex inside of this module ... */
	if (_nis_state.stayopen)
	  { /* use global state only if stayopen is set - otherwiese we would blow up getgrent_r() ... */
	     rv = __grscan_nis(retval, grp, buffer, buflen, &_nis_state, 1, NULL, gid);
	  }
	else
	  {
	    memset(&state, 0, sizeof(state));
	    rv = __grscan_nis(retval, grp, buffer, buflen, &state, 1, NULL, gid);
	    __grend_nis(&state);
	  }
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getgrnam(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_nis(&_nis_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_nis(&rerror, &_nis_group,
	    _nis_groupbuf, sizeof(_nis_groupbuf), &_nis_state, 1, name, 0);
	if (!_nis_state.stayopen)
		__grend_nis(&_nis_state);
	if (rv == NS_SUCCESS)
		*retval = &_nis_group;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getgrnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_nis state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
/* remark: we run under a global mutex inside of this module ... */
	if (_nis_state.stayopen)
	  { /* use global state only if stayopen is set - otherwiese we would blow up getgrent_r() ... */
	     rv = __grscan_nis(retval, grp, buffer, buflen, &_nis_state, 1, name, 0);
	  }
	else
	  {
	    memset(&state, 0, sizeof(state));
	    rv = __grscan_nis(retval, grp, buffer, buflen, &state, 1, name, 0);
	    __grend_nis(&state);
	  }
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

#endif /* YP */


#ifdef _GROUP_COMPAT
		/*
		 *	compat methods
		 */

int
__grstart_compat(struct __grstate_compat *state)
{

	_DIAGASSERT(state != NULL);

	if (state->fp == NULL) {
		state->fp = fopen(_PATH_GROUP, "re");
		if (state->fp == NULL)
			return NS_UNAVAIL;
	} else {
		rewind(state->fp);
	}
	return NS_SUCCESS;
}

int
__grend_compat(struct __grstate_compat *state)
{

	_DIAGASSERT(state != NULL);

	if (state->name) {
		free(state->name);
		state->name = NULL;
	}
	if (state->fp) {
		(void) fclose(state->fp);
		state->fp = NULL;
	}
	return NS_SUCCESS;
}


/*
 * __grbad_compat
 *	log an error if "files" or "compat" is specified in
 *	group_compat database
 */
/*ARGSUSED*/
int
__grbad_compat(void *nsrv, void *nscb, va_list ap)
{
	static int warned;

	_DIAGASSERT(nsrv != NULL);
	_DIAGASSERT(nscb != NULL);

	if (!warned) {
		syslog(LOG_ERR,
			"nsswitch.conf group_compat database can't use '%s'",
			(const char *)nscb);
	}
	warned = 1;
	return NS_UNAVAIL;
}

/*
 * __grscan_compat
 *	Scan state->fp for the next desired entry.
 *	If search is zero, return the next entry.
 *	If search is non-zero, look for a specific name (if name != NULL),
 *	or a specific gid (if name == NULL).
 *	Sets *retval to the errno if the result is not NS_SUCCESS or
 *	NS_NOTFOUND.
 *
 *	searchfunc is invoked when a compat "+" lookup is required;
 *	searchcookie is passed as the first argument to searchfunc,
 *	the second argument is the group result.
 *	This should return NS_NOTFOUND when "no more groups" from compat src.
 *	If searchfunc is NULL then nsdispatch of getgrent is used.
 *	This is primarily intended for getgroupmembership(3)'s compat backend.
 */
int
__grscan_compat(int *retval, struct group *grp, char *buffer, size_t buflen,
	struct __grstate_compat *state, int search, const char *name, gid_t gid,
	int (*searchfunc)(void *, struct group **), void *searchcookie)
{
	int		rv;
	char		filebuf[_GETGR_R_SIZE_MAX], *ep;

	static const ns_dtab compatentdtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_getgrent_r, NULL)
		NS_NIS_CB(_nis_getgrent_r, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};
	static const ns_dtab compatgiddtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_getgrgid_r, NULL)
		NS_NIS_CB(_nis_getgrgid_r, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};
	static const ns_dtab compatnamdtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_getgrnam_r, NULL)
		NS_NIS_CB(_nis_getgrnam_r, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	/* name is NULL to indicate searching for gid */

	*retval = 0;

	if (state->fp == NULL) {	/* only start if file not open yet */
		rv = __grstart_compat(state);
		if (rv != NS_SUCCESS)
			goto compatgrscan_out;
	}
	rv = NS_NOTFOUND;

	for (;;) {					/* loop through file */
		if (state->name != NULL) {
					/* processing compat entry */
			int		crv, cretval;
			struct group	cgrp, *cgrpres;

			if (state->name[0]) {		/* specific +group: */
				crv = nsdispatch(NULL, compatnamdtab,
				    NSDB_GROUP_COMPAT, "getgrnam_r",
				    __nsdefaultnis,
				    &cretval, state->name,
				    &cgrp, filebuf, sizeof(filebuf), &cgrpres);
				free(state->name);	/* (only check 1 grp) */
				state->name = NULL;
			} else if (!search) {		/* any group */
				if (searchfunc) {
					crv = searchfunc(searchcookie,
					    &cgrpres);
				} else {
					crv = nsdispatch(NULL, compatentdtab,
					    NSDB_GROUP_COMPAT, "getgrent_r",
					    __nsdefaultnis,
					    &cretval, &cgrp, filebuf,
					    sizeof(filebuf), &cgrpres);
				}
			} else if (name) {		/* specific group */
				crv = nsdispatch(NULL, compatnamdtab,
				    NSDB_GROUP_COMPAT, "getgrnam_r",
				    __nsdefaultnis,
				    &cretval, name,
				    &cgrp, filebuf, sizeof(filebuf), &cgrpres);
			} else {			/* specific gid */
				crv = nsdispatch(NULL, compatgiddtab,
				    NSDB_GROUP_COMPAT, "getgrgid_r",
				    __nsdefaultnis,
				    &cretval, gid,
				    &cgrp, filebuf, sizeof(filebuf), &cgrpres);
			}
			if (crv != NS_SUCCESS) {	/* not found */
				free(state->name);
				state->name = NULL;
				continue;		/* try next line */
			}
			if (!_gr_copy(cgrpres, grp, buffer, buflen)) {
				rv = NS_UNAVAIL;
				break;
			}
			goto compatgrscan_cmpgrp;	/* skip to grp test */
		}

							/* get next file line */
		if (fgets(filebuf, (int)sizeof(filebuf), state->fp) == NULL)
			break;

		ep = strchr(filebuf, '\n');
		if (ep == NULL) {	/* skip lines that are too big */
			int ch;

			while ((ch = getc(state->fp)) != '\n' && ch != EOF)
				continue;
			continue;
		}
		*ep = '\0';				/* clear trailing \n */

		if (filebuf[0] == '+') {		/* parse compat line */
			if (state->name)
				free(state->name);
			state->name = NULL;
			switch(filebuf[1]) {
			case ':':
			case '\0':
				state->name = strdup("");
				break;
			default:
				ep = strchr(filebuf + 1, ':');
				if (ep == NULL)
					break;
				*ep = '\0';
				state->name = strdup(filebuf + 1);
				break;
			}
			if (state->name == NULL) {
				rv = NS_UNAVAIL;
				break;
			}
			continue;
		}

							/* validate line */
		if (! _gr_parse(filebuf, grp, buffer, buflen)) {
			continue;			/* skip bad lines */
		}

 compatgrscan_cmpgrp:
		if (! search) {				/* just want this one */
			rv = NS_SUCCESS;
			break;
		}
							/* want specific */
		if ((name && strcmp(name, grp->gr_name) == 0) ||
		    (!name && gid == grp->gr_gid)) {
			rv = NS_SUCCESS;
			break;
		}

	}

 compatgrscan_out:
	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	return rv;
}

static struct __grstate_compat	_compat_state;
					/* storage for non _r functions */
static struct group		_compat_group;
static char			_compat_groupbuf[_GETGR_R_SIZE_MAX];

/*ARGSUSED*/
static int
_compat_setgrent(void *nsrv, void *nscb, va_list ap)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_setgrent, NULL)
		NS_NIS_CB(_nis_setgrent, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};

					/* force group_compat setgrent() */
	(void) nsdispatch(NULL, dtab, NSDB_GROUP_COMPAT, "setgrent",
	    __nsdefaultnis_forceall);

					/* reset state, keep fp open */
	_compat_state.stayopen = 0;
	return __grstart_compat(&_compat_state);
}

/*ARGSUSED*/
static int
_compat_setgroupent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_setgroupent, NULL)
		NS_NIS_CB(_nis_setgroupent, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};

					/* force group_compat setgroupent() */
	(void) nsdispatch(NULL, dtab, NSDB_GROUP_COMPAT, "setgroupent",
	    __nsdefaultnis_forceall, &rv, stayopen);

	_compat_state.stayopen = stayopen;
	rv = __grstart_compat(&_compat_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_compat_endgrent(void *nsrv, void *nscb, va_list ap)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(__grbad_compat, "files")
		NS_DNS_CB(_dns_endgrent, NULL)
		NS_NIS_CB(_nis_endgrent, NULL)
		NS_COMPAT_CB(__grbad_compat, "compat")
		NS_NULL_CB
	};

					/* force group_compat endgrent() */
	(void) nsdispatch(NULL, dtab, NSDB_GROUP_COMPAT, "endgrent",
	    __nsdefaultnis_forceall);

					/* reset state, close fp */
	_compat_state.stayopen = 0;
	return __grend_compat(&_compat_state);
}

/*ARGSUSED*/
static int
_compat_getgrent(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grscan_compat(&rerror, &_compat_group,
	    _compat_groupbuf, sizeof(_compat_groupbuf),
	    &_compat_state, 0, NULL, 0, NULL, NULL);
	if (rv == NS_SUCCESS)
		*retval = &_compat_group;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getgrent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	rv = __grscan_compat(retval, grp, buffer, buflen,
	    &_compat_state, 0, NULL, 0, NULL, NULL);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*result = NULL;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getgrgid(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	gid_t		 gid	= va_arg(ap, gid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_compat(&_compat_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_compat(&rerror, &_compat_group,
	    _compat_groupbuf, sizeof(_compat_groupbuf),
	    &_compat_state, 1, NULL, gid, NULL, NULL);
	if (!_compat_state.stayopen)
		__grend_compat(&_compat_state);
	if (rv == NS_SUCCESS)
		*retval = &_compat_group;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getgrgid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	gid_t		 gid	= va_arg(ap, gid_t);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_compat	state;
	int		rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = __grscan_compat(retval, grp, buffer, buflen, &state,
	    1, NULL, gid, NULL, NULL);
	__grend_compat(&state);
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getgrnam(void *nsrv, void *nscb, va_list ap)
{
	struct group	**retval = va_arg(ap, struct group **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = __grstart_compat(&_compat_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = __grscan_compat(&rerror, &_compat_group,
	    _compat_groupbuf, sizeof(_compat_groupbuf),
	    &_compat_state, 1, name, 0, NULL, NULL);
	if (!_compat_state.stayopen)
		__grend_compat(&_compat_state);
	if (rv == NS_SUCCESS)
		*retval = &_compat_group;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getgrnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	struct __grstate_compat	state;
	int		rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = __grscan_compat(retval, grp, buffer, buflen, &state,
	    1, name, 0, NULL, NULL);
	__grend_compat(&state);
	if (rv == NS_SUCCESS)
		*result = grp;
	return rv;
}

#endif	/* _GROUP_COMPAT */


		/*
		 *	public functions
		 */

struct group *
getgrent(void)
{
	int		rv;
	struct group	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgrent, NULL)
		NS_DNS_CB(_dns_getgrent, NULL)
		NS_NIS_CB(_nis_getgrent, NULL)
		NS_COMPAT_CB(_compat_getgrent, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__grmutex);
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "getgrent", __nsdefaultcompat,
	    &retval);
	mutex_unlock(&__grmutex);
	return (rv == NS_SUCCESS) ? retval : NULL;
}

int
getgrent_r(struct group *grp, char *buffer, size_t buflen,
    struct group **result)
{
	int		rv, retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgrent_r, NULL)
		NS_DNS_CB(_dns_getgrent_r, NULL)
		NS_NIS_CB(_nis_getgrent_r, NULL)
		NS_COMPAT_CB(_compat_getgrent_r, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__grmutex);
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "getgrent_r", __nsdefaultcompat,
	    &retval, grp, buffer, buflen, result);
	mutex_unlock(&__grmutex);
	switch (rv) {
	case NS_SUCCESS:
	case NS_NOTFOUND:
		return 0;
	default:
		return retval;
	}
}


struct group *
getgrgid(gid_t gid)
{
	int		rv;
	struct group	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgrgid, NULL)
		NS_DNS_CB(_dns_getgrgid, NULL)
		NS_NIS_CB(_nis_getgrgid, NULL)
		NS_COMPAT_CB(_compat_getgrgid, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__grmutex);
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "getgrgid", __nsdefaultcompat,
	    &retval, gid);
	mutex_unlock(&__grmutex);
	return (rv == NS_SUCCESS) ? retval : NULL;
}

int
getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t buflen,
	struct group **result)
{
	int	rv, retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgrgid_r, NULL)
		NS_DNS_CB(_dns_getgrgid_r, NULL)
		NS_NIS_CB(_nis_getgrgid_r, NULL)
		NS_COMPAT_CB(_compat_getgrgid_r, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	retval = 0;
	mutex_lock(&__grmutex);
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "getgrgid_r", __nsdefaultcompat,
	    &retval, gid, grp, buffer, buflen, result);
	mutex_unlock(&__grmutex);
	switch (rv) {
	case NS_SUCCESS:
	case NS_NOTFOUND:
		return 0;
	default:
		return retval;
	}
}

struct group *
getgrnam(const char *name)
{
	int		rv;
	struct group	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgrnam, NULL)
		NS_DNS_CB(_dns_getgrnam, NULL)
		NS_NIS_CB(_nis_getgrnam, NULL)
		NS_COMPAT_CB(_compat_getgrnam, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__grmutex);
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "getgrnam", __nsdefaultcompat,
	    &retval, name);
	mutex_unlock(&__grmutex);
	return (rv == NS_SUCCESS) ? retval : NULL;
}

int
getgrnam_r(const char *name, struct group *grp, char *buffer, size_t buflen,
	struct group **result)
{
	int	rv, retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getgrnam_r, NULL)
		NS_DNS_CB(_dns_getgrnam_r, NULL)
		NS_NIS_CB(_nis_getgrnam_r, NULL)
		NS_COMPAT_CB(_compat_getgrnam_r, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(name != NULL);
	_DIAGASSERT(grp != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	retval = 0;
	mutex_lock(&__grmutex);
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "getgrnam_r", __nsdefaultcompat,
	    &retval, name, grp, buffer, buflen, result);
	mutex_unlock(&__grmutex);
	switch (rv) {
	case NS_SUCCESS:
	case NS_NOTFOUND:
		return 0;
	default:
		return retval;
	}
}

void
endgrent(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_endgrent, NULL)
		NS_DNS_CB(_dns_endgrent, NULL)
		NS_NIS_CB(_nis_endgrent, NULL)
		NS_COMPAT_CB(_compat_endgrent, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__grmutex);
					/* force all endgrent() methods */
	(void) nsdispatch(NULL, dtab, NSDB_GROUP, "endgrent",
	    __nsdefaultcompat_forceall);
	mutex_unlock(&__grmutex);
}

int
setgroupent(int stayopen)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_setgroupent, NULL)
		NS_DNS_CB(_dns_setgroupent, NULL)
		NS_NIS_CB(_nis_setgroupent, NULL)
		NS_COMPAT_CB(_compat_setgroupent, NULL)
		NS_NULL_CB
	};
	int	rv, retval;

	mutex_lock(&__grmutex);
					/* force all setgroupent() methods */
	rv = nsdispatch(NULL, dtab, NSDB_GROUP, "setgroupent",
	    __nsdefaultcompat_forceall, &retval, stayopen);
	mutex_unlock(&__grmutex);
	return (rv == NS_SUCCESS) ? retval : 0;
}

void
setgrent(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_setgrent, NULL)
		NS_DNS_CB(_dns_setgrent, NULL)
		NS_NIS_CB(_nis_setgrent, NULL)
		NS_COMPAT_CB(_compat_setgrent, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__grmutex);
					/* force all setgrent() methods */
	(void) nsdispatch(NULL, dtab, NSDB_GROUP, "setgrent",
	    __nsdefaultcompat_forceall);
	mutex_unlock(&__grmutex);
}
