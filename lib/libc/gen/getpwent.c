/*	$NetBSD: getpwent.c,v 1.81 2012/09/08 15:15:06 dholland Exp $	*/

/*-
 * Copyright (c) 1997-2000, 2004-2005 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1988, 1993
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
 * Portions Copyright (c) 1994, 1995, Jason Downs.  All rights reserved.
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
static char sccsid[] = "@(#)getpwent.c	8.2 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: getpwent.c,v 1.81 2012/09/08 15:15:06 dholland Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"

#include <sys/param.h>

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netgroup.h>
#include <nsswitch.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#ifdef HESIOD
#include <hesiod.h>
#endif

#ifdef YP
#include <machine/param.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#include "pw_private.h"

#define	_PASSWD_COMPAT	/* "passwd" defaults to compat, so always provide it */

#ifdef __weak_alias
__weak_alias(endpwent,_endpwent)
__weak_alias(setpassent,_setpassent)
__weak_alias(setpwent,_setpwent)
#endif

#ifdef _REENTRANT
static 	mutex_t			_pwmutex = MUTEX_INITIALIZER;
#endif

const char __yp_token[] = "__YP!";	/* Let pwd_mkdb pull this in. */


/*
 * The pwd.db lookup techniques and data extraction code here must be kept
 * in sync with that in `pwd_mkdb'.
 */

#if defined(YP) || defined(HESIOD)
/*
 * _pw_parse
 *	Parses entry using pw_scan(3) (without the trailing \n)
 *	after copying to buf, and fills in pw with corresponding values.
 *	If old is non-zero, entry is in _PASSWORD_OLDFMT.
 *	Returns 1 if parsed successfully, 0 on parse failure.
 */
static int
_pw_parse(const char *entry, struct passwd *pw, char *buf, size_t buflen,
	int old)
{
	int	flags;

	_DIAGASSERT(entry != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buf != NULL);

	if (strlcpy(buf, entry, buflen) >= buflen)
		return 0;
	flags = _PASSWORD_NOWARN;
	if (old)
		flags |= _PASSWORD_OLDFMT;
	return __pw_scan(buf, pw, &flags);
}
#endif /* YP || HESIOD */

/*
 * _pw_opendb
 *	if *db is NULL, dbopen(3) /etc/spwd.db or /etc/pwd.db (depending
 *	upon permissions, etc)
 */
static int
_pw_opendb(DB **db, int *version)
{
	static int	warned;
	DBT		key;
	DBT		value;

	const char	*dbfile = NULL;

	_DIAGASSERT(db != NULL);
	_DIAGASSERT(version != NULL);
	if (*db != NULL)					/* open *db */
		return NS_SUCCESS;

	if (geteuid() == 0) {
		dbfile = _PATH_SMP_DB;
		*db = dbopen(dbfile, O_RDONLY, 0, DB_HASH, NULL);
	}
	if (*db == NULL) {
		dbfile = _PATH_MP_DB;
		*db = dbopen(dbfile, O_RDONLY, 0, DB_HASH, NULL);
	}
	if (*db == NULL) {
		if (!warned) {
			int	serrno = errno;
			syslog(LOG_ERR, "%s: %m", dbfile);
			errno = serrno;
		}
		warned = 1;
		return NS_UNAVAIL;
	}
	key.data = __UNCONST("VERSION");
	key.size = strlen((char *)key.data) + 1;
	switch ((*(*db)->get)(*db, &key, &value, 0)) {
	case 0:
		if (sizeof(*version) != value.size)
			return NS_UNAVAIL;
		(void)memcpy(version, value.data, value.size);
		break;			/* found */
	case 1:
		*version = 0;		/* not found */
		break;
	case -1:
		return NS_UNAVAIL;	/* error in db routines */
	default:
		abort();
	}
	return NS_SUCCESS;
}

/*
 * _pw_getkey
 *	Lookup key in *db, filling in pw
 *	with the result, allocating memory from buffer (size buflen).
 *	(The caller may point key.data to buffer on entry; the contents
 *	of key.data will be invalid on exit.)
 */
static int
_pw_getkey(DB *db, DBT *key,
	struct passwd *pw, char *buffer, size_t buflen, int *pwflags,
	int version)
{
	char		*p, *t;
	DBT		data;

	_DIAGASSERT(db != NULL);
	_DIAGASSERT(key != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	/* pwflags may be NULL (if we don't care about them */

	if (db == NULL)			/* this shouldn't happen */
		return NS_UNAVAIL;

	switch ((db->get)(db, key, &data, 0)) {
	case 0:
		break;			/* found */
	case 1:
		return NS_NOTFOUND;	/* not found */
	case -1:
		return NS_UNAVAIL;	/* error in db routines */
	default:
		abort();
	}

	p = (char *)data.data;
	if (data.size > buflen) {
		errno = ERANGE;
		return NS_UNAVAIL;
	}

			/*
			 * THE DECODING BELOW MUST MATCH THAT IN pwd_mkdb.
			 */
	t = buffer;
#define MACRO(a)	do { a } while (/*CONSTCOND*/0)
#define	EXPAND(e)	MACRO(e = t; while ((*t++ = *p++));)
#define	SCALAR(v)	MACRO(memmove(&(v), p, sizeof v); p += sizeof v;)
	EXPAND(pw->pw_name);
	EXPAND(pw->pw_passwd);
	SCALAR(pw->pw_uid);
	SCALAR(pw->pw_gid);
	if (version == 0) {
		int32_t tmp;
		SCALAR(tmp);
		pw->pw_change = tmp;
	} else
		SCALAR(pw->pw_change);
	EXPAND(pw->pw_class);
	EXPAND(pw->pw_gecos);
	EXPAND(pw->pw_dir);
	EXPAND(pw->pw_shell);
	if (version == 0) {
		int32_t tmp;
		SCALAR(tmp);
		pw->pw_expire = tmp;
	} else
		SCALAR(pw->pw_expire);
	if (pwflags) {
		/* See if there's any data left.  If so, read in flags. */
		if (data.size > (size_t) (p - (char *)data.data)) {
			SCALAR(*pwflags);
		} else {				/* default */
			*pwflags = _PASSWORD_NOUID|_PASSWORD_NOGID;
		}
	}

	return NS_SUCCESS;
}

/*
 * _pw_memfrombuf
 *	Obtain want bytes from buffer (of size buflen) and return a pointer
 *	to the available memory after adjusting buffer/buflen.
 *	Returns NULL if there is insufficient space.
 */
static char *
_pw_memfrombuf(size_t want, char **buffer, size_t *buflen)
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
 * _pw_copy
 *	Copy the contents of frompw to pw; memory for strings
 *	and arrays will be allocated from buf (of size buflen).
 *	If proto != NULL, use various fields in proto in preference to frompw.
 *	Returns 1 if copied successfully, 0 on copy failure.
 *	NOTE: frompw must not use buf for its own pointers.
 */
static int
_pw_copy(const struct passwd *frompw, struct passwd *pw,
	char *buf, size_t buflen, const struct passwd *protopw, int protoflags)
{
	size_t	count;
	int	useproto;

	_DIAGASSERT(frompw != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buf != NULL);
	/* protopw may be NULL */

	useproto = protopw && protopw->pw_name;

#define	COPYSTR(to, from) \
	do { \
		count = strlen((from)); \
		(to) = _pw_memfrombuf(count+1, &buf, &buflen); \
		if ((to) == NULL) \
			return 0; \
		memmove((to), (from), count); \
		to[count] = '\0'; \
	} while (0)	/* LINTED */

#define	COPYFIELD(field)	COPYSTR(pw->field, frompw->field)

#define	COPYPROTOFIELD(field)	COPYSTR(pw->field, \
		(useproto && *protopw->field ? protopw->field : frompw->field))

	COPYFIELD(pw_name);

#ifdef PW_OVERRIDE_PASSWD
	COPYPROTOFIELD(pw_passwd);
#else
	COPYFIELD(pw_passwd);
#endif

	if (useproto && !(protoflags & _PASSWORD_NOUID))
		pw->pw_uid = protopw->pw_uid;
	else
		pw->pw_uid = frompw->pw_uid;

	if (useproto && !(protoflags & _PASSWORD_NOGID))
		pw->pw_gid = protopw->pw_gid;
	else
		pw->pw_gid = frompw->pw_gid;

	pw->pw_change = frompw->pw_change;
	COPYFIELD(pw_class);
	COPYPROTOFIELD(pw_gecos);
	COPYPROTOFIELD(pw_dir);
	COPYPROTOFIELD(pw_shell);

#undef COPYSTR
#undef COPYFIELD
#undef COPYPROTOFIELD

	return 1;
}


		/*
		 *	files methods
		 */

	/* state shared between files methods */
struct files_state {
	int	 stayopen;		/* see getpassent(3) */
	DB	*db;			/* passwd file handle */
	int	 keynum;		/* key counter, -1 if no more */
	int	 version;
};

static struct files_state	_files_state;
					/* storage for non _r functions */
static struct passwd		_files_passwd;
static char			_files_passwdbuf[_GETPW_R_SIZE_MAX];

static int
_files_start(struct files_state *state)
{
	int	rv;

	_DIAGASSERT(state != NULL);

	state->keynum = 0;
	rv = _pw_opendb(&state->db, &state->version);
	if (rv != NS_SUCCESS)
		return rv;
	return NS_SUCCESS;
}

static int
_files_end(struct files_state *state)
{

	_DIAGASSERT(state != NULL);

	state->keynum = 0;
	if (state->db) {
		(void)(state->db->close)(state->db);
		state->db = NULL;
	}
	return NS_SUCCESS;
}

/*
 * _files_pwscan
 *	Search state->db for the next desired entry.
 *	If search is _PW_KEYBYNUM, look for state->keynum.
 *	If search is _PW_KEYBYNAME, look for name.
 *	If search is _PW_KEYBYUID, look for uid.
 *	Sets *retval to the errno if the result is not NS_SUCCESS
 *	or NS_NOTFOUND.
 */
static int
_files_pwscan(int *retval, struct passwd *pw, char *buffer, size_t buflen,
	struct files_state *state, int search, const char *name, uid_t uid)
{
	const void	*from;
	size_t		 fromlen;
	DBT		 key;
	int		 rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	/* name is NULL to indicate searching for uid */

	*retval = 0;

	if (state->db == NULL) {	/* only start if file not open yet */
		rv = _files_start(state);
		if (rv != NS_SUCCESS)
			goto filespwscan_out;
	}

	for (;;) {				/* search for a match */
		switch (search) {
		case _PW_KEYBYNUM:
			if (state->keynum == -1)
				return NS_NOTFOUND;	/* no more records */
			state->keynum++;
			from = &state->keynum;
			fromlen = sizeof(state->keynum);
			break;
		case _PW_KEYBYNAME:
			from = name;
			fromlen = strlen(name);
			break;
		case _PW_KEYBYUID:
			from = &uid;
			fromlen = sizeof(uid);
			break;
		default:
			abort();
		}

		if (buflen <= fromlen) {		/* buffer too small */
			*retval = ERANGE;
			return NS_UNAVAIL;
		}
		buffer[0] = search;			/* setup key */
		memmove(buffer + 1, from, fromlen);
		key.size = fromlen + 1;
		key.data = (u_char *)buffer;

							/* search for key */
		rv = _pw_getkey(state->db, &key, pw, buffer, buflen, NULL,
		    state->version);
		if (rv != NS_SUCCESS)			/* no match */
			break;
		if (pw->pw_name[0] == '+' || pw->pw_name[0] == '-') {
						/* if a compat line */
			if (search == _PW_KEYBYNUM)
				continue;	/* read next if pwent */
			rv = NS_NOTFOUND;	/* don't match if pw{nam,uid} */
			break;
		}
		break;
	}

	if (rv == NS_NOTFOUND && search == _PW_KEYBYNUM)
		state->keynum = -1;		/* flag `no more records' */

	if (rv == NS_SUCCESS) {
		if ((search == _PW_KEYBYUID && pw->pw_uid != uid) ||
		    (search == _PW_KEYBYNAME && strcmp(pw->pw_name, name) != 0))
			rv = NS_NOTFOUND;
	}

 filespwscan_out:
	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	return rv;
}

/*ARGSUSED*/
static int
_files_setpwent(void *nsrv, void *nscb, va_list ap)
{

	_files_state.stayopen = 0;
	return _files_start(&_files_state);
}

/*ARGSUSED*/
static int
_files_setpassent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	_files_state.stayopen = stayopen;
	rv = _files_start(&_files_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_files_endpwent(void *nsrv, void *nscb, va_list ap)
{

	_files_state.stayopen = 0;
	return _files_end(&_files_state);
}

/*ARGSUSED*/
static int
_files_getpwent(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _files_pwscan(&rerror, &_files_passwd,
	    _files_passwdbuf, sizeof(_files_passwdbuf),
	    &_files_state, _PW_KEYBYNUM, NULL, 0);
	if (rv == NS_SUCCESS)
		*retval = &_files_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_files_getpwent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	rv = _files_pwscan(retval, pw, buffer, buflen, &_files_state,
	    _PW_KEYBYNUM, NULL, 0);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*result = NULL;
	return rv;
}

/*ARGSUSED*/
static int
_files_getpwnam(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _files_start(&_files_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = _files_pwscan(&rerror, &_files_passwd,
	    _files_passwdbuf, sizeof(_files_passwdbuf),
	    &_files_state, _PW_KEYBYNAME, name, 0);
	if (!_files_state.stayopen)
		_files_end(&_files_state);
	if (rv == NS_SUCCESS)
		*retval = &_files_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_files_getpwnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct files_state state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = _files_pwscan(retval, pw, buffer, buflen, &state,
	    _PW_KEYBYNAME, name, 0);
	_files_end(&state);
	if (rv == NS_SUCCESS)
		*result = pw;
	return rv;
}

/*ARGSUSED*/
static int
_files_getpwuid(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	uid_t		 uid	= va_arg(ap, uid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _files_start(&_files_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = _files_pwscan(&rerror, &_files_passwd,
	    _files_passwdbuf, sizeof(_files_passwdbuf),
	    &_files_state, _PW_KEYBYUID, NULL, uid);
	if (!_files_state.stayopen)
		_files_end(&_files_state);
	if (rv == NS_SUCCESS)
		*retval = &_files_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_files_getpwuid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	uid_t		 uid	= va_arg(ap, uid_t);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct files_state state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = _files_pwscan(retval, pw, buffer, buflen, &state,
	    _PW_KEYBYUID, NULL, uid);
	_files_end(&state);
	if (rv == NS_SUCCESS)
		*result = pw;
	return rv;
}


#ifdef HESIOD
		/*
		 *	dns methods
		 */

	/* state shared between dns methods */
struct dns_state {
	int	 stayopen;		/* see getpassent(3) */
	void	*context;		/* Hesiod context */
	int	 num;			/* passwd index, -1 if no more */
};

static struct dns_state		_dns_state;
					/* storage for non _r functions */
static struct passwd		_dns_passwd;
static char			_dns_passwdbuf[_GETPW_R_SIZE_MAX];

static int
_dns_start(struct dns_state *state)
{

	_DIAGASSERT(state != NULL);

	state->num = 0;
	if (state->context == NULL) {			/* setup Hesiod */
		if (hesiod_init(&state->context) == -1)
			return NS_UNAVAIL;
	}

	return NS_SUCCESS;
}

static int
_dns_end(struct dns_state *state)
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
 * _dns_pwscan
 *	Look for the Hesiod name provided in buffer in the NULL-terminated
 *	list of zones,
 *	and decode into pw/buffer/buflen.
 */
static int
_dns_pwscan(int *retval, struct passwd *pw, char *buffer, size_t buflen,
	struct dns_state *state, const char **zones)
{
	const char	**curzone;
	char		**hp, *ep;
	int		rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	_DIAGASSERT(zones != NULL);

	*retval = 0;

	if (state->context == NULL) {	/* only start if Hesiod not setup */
		rv = _dns_start(state);
		if (rv != NS_SUCCESS)
			return rv;
	}

	hp = NULL;
	rv = NS_NOTFOUND;

	for (curzone = zones; *curzone; curzone++) {	/* search zones */
		hp = hesiod_resolve(state->context, buffer, *curzone);
		if (hp != NULL)
			break;
		if (errno != ENOENT) {
			rv = NS_UNAVAIL;
			goto dnspwscan_out;
		}
	}
	if (*curzone == NULL)
		goto dnspwscan_out;

	if ((ep = strchr(hp[0], '\n')) != NULL)
		*ep = '\0';				/* clear trailing \n */
	if (_pw_parse(hp[0], pw, buffer, buflen, 1))	/* validate line */
		rv = NS_SUCCESS;
	else
		rv = NS_UNAVAIL;

 dnspwscan_out:
	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	if (hp)
		hesiod_free_list(state->context, hp);
	return rv;
}

/*ARGSUSED*/
static int
_dns_setpwent(void *nsrv, void *nscb, va_list ap)
{

	_dns_state.stayopen = 0;
	return _dns_start(&_dns_state);
}

/*ARGSUSED*/
static int
_dns_setpassent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	_dns_state.stayopen = stayopen;
	rv = _dns_start(&_dns_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_dns_endpwent(void *nsrv, void *nscb, va_list ap)
{

	_dns_state.stayopen = 0;
	return _dns_end(&_dns_state);
}

/*ARGSUSED*/
static int
_dns_getpwent(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);

	char	**hp, *ep;
	int	  rv;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;

	if (_dns_state.num == -1)			/* exhausted search */
		return NS_NOTFOUND;

	if (_dns_state.context == NULL) {
			/* only start if Hesiod not setup */
		rv = _dns_start(&_dns_state);
		if (rv != NS_SUCCESS)
			return rv;
	}

 next_dns_entry:
	hp = NULL;
	rv = NS_NOTFOUND;

							/* find passwd-NNN */
	snprintf(_dns_passwdbuf, sizeof(_dns_passwdbuf),
	    "passwd-%u", _dns_state.num);
	_dns_state.num++;

	hp = hesiod_resolve(_dns_state.context, _dns_passwdbuf, "passwd");
	if (hp == NULL) {
		if (errno == ENOENT)
			_dns_state.num = -1;
		else
			rv = NS_UNAVAIL;
	} else {
		if ((ep = strchr(hp[0], '\n')) != NULL)
			*ep = '\0';			/* clear trailing \n */
							/* validate line */
		if (_pw_parse(hp[0], &_dns_passwd,
		    _dns_passwdbuf, sizeof(_dns_passwdbuf), 1))
			rv = NS_SUCCESS;
		else {				/* dodgy entry, try again */
			hesiod_free_list(_dns_state.context, hp);
			goto next_dns_entry;
		}
	}

	if (hp)
		hesiod_free_list(_dns_state.context, hp);
	if (rv == NS_SUCCESS)
		*retval = &_dns_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getpwent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	char	**hp, *ep;
	int	  rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*retval = 0;

	if (_dns_state.num == -1)			/* exhausted search */
		return NS_NOTFOUND;

	if (_dns_state.context == NULL) {
			/* only start if Hesiod not setup */
		rv = _dns_start(&_dns_state);
		if (rv != NS_SUCCESS)
			return rv;
	}

 next_dns_entry:
	hp = NULL;
	rv = NS_NOTFOUND;

							/* find passwd-NNN */
	snprintf(buffer, buflen, "passwd-%u", _dns_state.num);
	_dns_state.num++;

	hp = hesiod_resolve(_dns_state.context, buffer, "passwd");
	if (hp == NULL) {
		if (errno == ENOENT)
			_dns_state.num = -1;
		else
			rv = NS_UNAVAIL;
	} else {
		if ((ep = strchr(hp[0], '\n')) != NULL)
			*ep = '\0';			/* clear trailing \n */
							/* validate line */
		if (_pw_parse(hp[0], pw, buffer, buflen, 1))
			rv = NS_SUCCESS;
		else {				/* dodgy entry, try again */
			hesiod_free_list(_dns_state.context, hp);
			goto next_dns_entry;
		}
	}

	if (hp)
		hesiod_free_list(_dns_state.context, hp);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*result = NULL;
	return rv;
}

static const char *_dns_uid_zones[] = {
	"uid",
	"passwd",
	NULL
};

/*ARGSUSED*/
static int
_dns_getpwuid(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	uid_t		 uid	= va_arg(ap, uid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _dns_start(&_dns_state);
	if (rv != NS_SUCCESS)
		return rv;
	snprintf(_dns_passwdbuf, sizeof(_dns_passwdbuf),
	    "%u", (unsigned int)uid);
	rv = _dns_pwscan(&rerror, &_dns_passwd,
	    _dns_passwdbuf, sizeof(_dns_passwdbuf),
	    &_dns_state, _dns_uid_zones);
	if (!_dns_state.stayopen)
		_dns_end(&_dns_state);
	if (rv == NS_SUCCESS && uid == _dns_passwd.pw_uid)
		*retval = &_dns_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getpwuid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	uid_t		 uid	= va_arg(ap, uid_t);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct dns_state state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	snprintf(buffer, buflen, "%u", (unsigned int)uid);
	rv = _dns_pwscan(retval, pw, buffer, buflen, &state, _dns_uid_zones);
	_dns_end(&state);
	if (rv != NS_SUCCESS)
		return rv;
	if (uid == pw->pw_uid) {
		*result = pw;
		return NS_SUCCESS;
	} else
		return NS_NOTFOUND;
}

static const char *_dns_nam_zones[] = {
	"passwd",
	NULL
};

/*ARGSUSED*/
static int
_dns_getpwnam(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _dns_start(&_dns_state);
	if (rv != NS_SUCCESS)
		return rv;
	snprintf(_dns_passwdbuf, sizeof(_dns_passwdbuf), "%s", name);
	rv = _dns_pwscan(&rerror, &_dns_passwd,
	    _dns_passwdbuf, sizeof(_dns_passwdbuf),
	    &_dns_state, _dns_nam_zones);
	if (!_dns_state.stayopen)
		_dns_end(&_dns_state);
	if (rv == NS_SUCCESS && strcmp(name, _dns_passwd.pw_name) == 0)
		*retval = &_dns_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_dns_getpwnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct dns_state state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	snprintf(buffer, buflen, "%s", name);
	rv = _dns_pwscan(retval, pw, buffer, buflen, &state, _dns_nam_zones);
	_dns_end(&state);
	if (rv != NS_SUCCESS)
		return rv;
	if (strcmp(name, pw->pw_name) == 0) {
		*result = pw;
		return NS_SUCCESS;
	} else
		return NS_NOTFOUND;
}

#endif /* HESIOD */


#ifdef YP
		/*
		 *	nis methods
		 */
	/* state shared between nis methods */
struct nis_state {
	int		 stayopen;	/* see getpassent(3) */
	char		*domain;	/* NIS domain */
	int		 done;		/* non-zero if search exhausted */
	char		*current;	/* current first/next match */
	int		 currentlen;	/* length of _nis_current */
	enum {				/* shadow map type */
		NISMAP_UNKNOWN = 0,	/*  unknown ... */
		NISMAP_NONE,		/*  none: use "passwd.by*" */
		NISMAP_ADJUNCT,		/*  pw_passwd from "passwd.adjunct.*" */
		NISMAP_MASTER		/*  all from "master.passwd.by*" */
	}		 maptype;
};

static struct nis_state		_nis_state;
					/* storage for non _r functions */
static struct passwd		_nis_passwd;
static char			_nis_passwdbuf[_GETPW_R_SIZE_MAX];

static const char __nis_pw_n_1[] = "master.passwd.byname";
static const char __nis_pw_n_2[] = "passwd.byname";
static const char __nis_pw_u_1[] = "master.passwd.byuid";
static const char __nis_pw_u_2[] = "passwd.byuid";

static const char * const __nis_pw_n_map[4] = { __nis_pw_n_2, __nis_pw_n_2, __nis_pw_n_2, __nis_pw_n_1 };
static const char * const __nis_pw_u_map[4] = { __nis_pw_u_2, __nis_pw_u_2, __nis_pw_u_2, __nis_pw_u_1 };

	/* macros for deciding which NIS maps to use. */
#define	PASSWD_BYNAME(x)	((x)->maptype == NISMAP_MASTER ? __nis_pw_n_1 : __nis_pw_n_2)
#define	PASSWD_BYUID(x)		((x)->maptype == NISMAP_MASTER ? __nis_pw_u_1 : __nis_pw_u_2)

static int
_nis_start(struct nis_state *state)
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

				/* determine where to get pw_passwd from */
	if (state->maptype == NISMAP_UNKNOWN) {
		int	r, order;

		state->maptype = NISMAP_NONE;	/* default to no adjunct */
		if (geteuid() != 0)		/* non-root can't use adjunct */
			return NS_SUCCESS;

						/* look for "master.passwd.*" */
		r = yp_order(state->domain, "master.passwd.byname", &order);
		if (r == 0) {
			state->maptype = NISMAP_MASTER;
			return NS_SUCCESS;
		}

			/* master.passwd doesn't exist, try passwd.adjunct */
		if (r == YPERR_MAP) {
			r = yp_order(state->domain, "passwd.adjunct.byname",
			    &order);
			if (r == 0)
				state->maptype = NISMAP_ADJUNCT;
		}
	}
	return NS_SUCCESS;
}

static int
_nis_end(struct nis_state *state)
{

	_DIAGASSERT(state != NULL);

	if (state->domain)
		state->domain = NULL;
	state->done = 0;
	if (state->current)
		free(state->current);
	state->current = NULL;
	state->maptype = NISMAP_UNKNOWN;
	return NS_SUCCESS;
}

/*
 * nis_parse
 *	wrapper to _pw_parse that obtains the real password from the
 *	"passwd.adjunct.byname" NIS map if the maptype is NISMAP_ADJUNCT.
 */
static int
_nis_parse(const char *entry, struct passwd *pw, char *buf, size_t buflen,
	struct nis_state *state)
{
	size_t	elen;

	_DIAGASSERT(entry != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buf != NULL);
	_DIAGASSERT(state != NULL);

	elen = strlen(entry) + 1;
	if (elen >= buflen)
		return 0;
	if (! _pw_parse(entry, pw, buf, buflen,
	    !(state->maptype == NISMAP_MASTER)))
		return 0;

	if ((state->maptype == NISMAP_ADJUNCT) &&
	    (strstr(pw->pw_passwd, "##") != NULL)) {
		char	*data;
		int	datalen;

		if (yp_match(state->domain, "passwd.adjunct.byname",
		    pw->pw_name, (int)strlen(pw->pw_name),
		    &data, &datalen) == 0) {
			char	*bp, *ep;
						/* skip name to get password */
			ep = data;
			if (strsep(&ep, ":") != NULL &&
			    (bp = strsep(&ep, ":")) != NULL) {
					/* store new pw_passwd after entry */
				if (strlcpy(buf + elen, bp, buflen - elen) >=
				    buflen - elen) {
					free(data);
					return 0;
				}
				pw->pw_passwd = &buf[elen];
			}
			free(data);
		}
	}

	return 1;
}


/*
 * _nis_pwscan
 *	Look for the yp key provided in buffer from map,
 *	and decode into pw/buffer/buflen.
 */
static int
_nis_pwscan(int *retval, struct passwd *pw, char *buffer, size_t buflen,
	struct nis_state *state, const char * const *map_arr, size_t nmaps)
{
	char	*data;
	int	nisr, rv, datalen;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	_DIAGASSERT(map_arr != NULL);

	*retval = 0;

	if (state->domain == NULL) {	/* only start if NIS not setup */
		rv = _nis_start(state);
		if (rv != NS_SUCCESS)
			return rv;
	}

	data = NULL;
	rv = NS_NOTFOUND;
	_DIAGASSERT(state->maptype != NISMAP_UNKNOWN &&
		    (unsigned)state->maptype < nmaps);

							/* search map */
	nisr = yp_match(state->domain, map_arr[state->maptype], buffer, (int)strlen(buffer),
	    &data, &datalen);
	switch (nisr) {
	case 0:
		data[datalen] = '\0';			/* clear trailing \n */
		if (_nis_parse(data, pw, buffer, buflen, state))
			rv = NS_SUCCESS;		/* validate line */
		else
			rv = NS_UNAVAIL;
		break;
	case YPERR_KEY:
		break;
	default:
		rv = NS_UNAVAIL;
		break;
	}

	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	if (data)
		free(data);
	return rv;
}

/*ARGSUSED*/
static int
_nis_setpwent(void *nsrv, void *nscb, va_list ap)
{

	_nis_state.stayopen = 0;
	return _nis_start(&_nis_state);
}

/*ARGSUSED*/
static int
_nis_setpassent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

	_nis_state.stayopen = stayopen;
	rv = _nis_start(&_nis_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_nis_endpwent(void *nsrv, void *nscb, va_list ap)
{

	return _nis_end(&_nis_state);
}


/*ARGSUSED*/
static int
_nis_getpwent(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);

	char	*key, *data;
	int	keylen, datalen, rv, nisr;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;

	if (_nis_state.done)				/* exhausted search */
		return NS_NOTFOUND;
	if (_nis_state.domain == NULL) {
					/* only start if NIS not setup */
		rv = _nis_start(&_nis_state);
		if (rv != NS_SUCCESS)
			return rv;
	}

 next_nis_entry:
	key = NULL;
	data = NULL;
	rv = NS_NOTFOUND;

	if (_nis_state.current) {			/* already searching */
		nisr = yp_next(_nis_state.domain, PASSWD_BYNAME(&_nis_state),
		    _nis_state.current, _nis_state.currentlen,
		    &key, &keylen, &data, &datalen);
		free(_nis_state.current);
		_nis_state.current = NULL;
		switch (nisr) {
		case 0:
			_nis_state.current = key;
			_nis_state.currentlen = keylen;
			key = NULL;
			break;
		case YPERR_NOMORE:
			_nis_state.done = 1;
			goto nisent_out;
		default:
			rv = NS_UNAVAIL;
			goto nisent_out;
		}
	} else {					/* new search */
		if (yp_first(_nis_state.domain, PASSWD_BYNAME(&_nis_state),
		    &_nis_state.current, &_nis_state.currentlen,
		    &data, &datalen)) {
			rv = NS_UNAVAIL;
			goto nisent_out;
		}
	}

	data[datalen] = '\0';				/* clear trailing \n */
							/* validate line */
	if (_nis_parse(data, &_nis_passwd,
	    _nis_passwdbuf, sizeof(_nis_passwdbuf), &_nis_state))
		rv = NS_SUCCESS;
	else {					/* dodgy entry, try again */
		free(data);
		goto next_nis_entry;
	}

 nisent_out:
	if (key)
		free(key);
	if (data)
		free(data);
	if (rv == NS_SUCCESS)
		*retval = &_nis_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getpwent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	char	*key, *data;
	int	keylen, datalen, rv, nisr;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*retval = 0;

	if (_nis_state.done)				/* exhausted search */
		return NS_NOTFOUND;
	if (_nis_state.domain == NULL) {
					/* only start if NIS not setup */
		rv = _nis_start(&_nis_state);
		if (rv != NS_SUCCESS)
			return rv;
	}

 next_nis_entry:
	key = NULL;
	data = NULL;
	rv = NS_NOTFOUND;

	if (_nis_state.current) {			/* already searching */
		nisr = yp_next(_nis_state.domain, PASSWD_BYNAME(&_nis_state),
		    _nis_state.current, _nis_state.currentlen,
		    &key, &keylen, &data, &datalen);
		free(_nis_state.current);
		_nis_state.current = NULL;
		switch (nisr) {
		case 0:
			_nis_state.current = key;
			_nis_state.currentlen = keylen;
			key = NULL;
			break;
		case YPERR_NOMORE:
			_nis_state.done = 1;
			goto nisent_out;
		default:
			rv = NS_UNAVAIL;
			goto nisent_out;
		}
	} else {					/* new search */
		if (yp_first(_nis_state.domain, PASSWD_BYNAME(&_nis_state),
		    &_nis_state.current, &_nis_state.currentlen,
		    &data, &datalen)) {
			rv = NS_UNAVAIL;
			goto nisent_out;
		}
	}

	data[datalen] = '\0';				/* clear trailing \n */
							/* validate line */
	if (_nis_parse(data, pw, buffer, buflen, &_nis_state))
		rv = NS_SUCCESS;
	else {					/* dodgy entry, try again */
		if (key)
			free(key);
		free(data);
		goto next_nis_entry;
	}

 nisent_out:
	if (key)
		free(key);
	if (data)
		free(data);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*result = NULL;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getpwuid(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	uid_t		 uid	= va_arg(ap, uid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _nis_start(&_nis_state);
	if (rv != NS_SUCCESS)
		return rv;
	snprintf(_nis_passwdbuf, sizeof(_nis_passwdbuf), "%u", (unsigned int)uid);
	rv = _nis_pwscan(&rerror, &_nis_passwd,
	    _nis_passwdbuf, sizeof(_nis_passwdbuf),
	    &_nis_state, __nis_pw_u_map, __arraycount(__nis_pw_u_map));
	if (!_nis_state.stayopen)
		_nis_end(&_nis_state);
	if (rv == NS_SUCCESS && uid == _nis_passwd.pw_uid)
		*retval = &_nis_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getpwuid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	uid_t		 uid	= va_arg(ap, uid_t);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct nis_state state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	snprintf(buffer, buflen, "%u", (unsigned int)uid);
/* remark: we run under a global mutex inside of this module ... */
	if (_nis_state.stayopen)
	  { /* use global state only if stayopen is set - otherwise we would blow up getpwent_r() ... */
	    rv = _nis_pwscan(retval, pw, buffer, buflen,
		&_nis_state, __nis_pw_u_map, __arraycount(__nis_pw_u_map));
	  }
	else
	  { /* keep old semantic if no stayopen set - no need to call _nis_start() here - _nis_pwscan() will do it for us ... */
	    /* use same way as in getgrent.c ... */
	    memset(&state, 0, sizeof(state));
	    rv = _nis_pwscan(retval, pw, buffer, buflen,
		&state, __nis_pw_u_map, __arraycount(__nis_pw_u_map));
	    _nis_end(&state);
	  }
	if (rv != NS_SUCCESS)
		return rv;
	if (uid == pw->pw_uid) {
		*result = pw;
		return NS_SUCCESS;
	} else
		return NS_NOTFOUND;
}

/*ARGSUSED*/
static int
_nis_getpwnam(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _nis_start(&_nis_state);
	if (rv != NS_SUCCESS)
		return rv;
	snprintf(_nis_passwdbuf, sizeof(_nis_passwdbuf), "%s", name);
	rv = _nis_pwscan(&rerror, &_nis_passwd,
	    _nis_passwdbuf, sizeof(_nis_passwdbuf),
	    &_nis_state, __nis_pw_n_map, __arraycount(__nis_pw_n_map));
	if (!_nis_state.stayopen)
		_nis_end(&_nis_state);
	if (rv == NS_SUCCESS && strcmp(name, _nis_passwd.pw_name) == 0)
		*retval = &_nis_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_nis_getpwnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct nis_state state;
	int	rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	snprintf(buffer, buflen, "%s", name);
/* remark: we run under a global mutex inside of this module ... */
	if (_nis_state.stayopen)
	  { /* use global state only if stayopen is set - otherwise we would blow up getpwent_r() ... */
	    rv = _nis_pwscan(retval, pw, buffer, buflen,
		&_nis_state, __nis_pw_n_map, __arraycount(__nis_pw_n_map));
	  }
	else
	  { /* keep old semantic if no stayopen set - no need to call _nis_start() here - _nis_pwscan() will do it for us ... */
	    /* use same way as in getgrent.c ... */
	    memset(&state, 0, sizeof(state));
	    rv = _nis_pwscan(retval, pw, buffer, buflen,
		&state, __nis_pw_n_map, __arraycount(__nis_pw_n_map));
	    _nis_end(&state);
	  }
	if (rv != NS_SUCCESS)
		return rv;
	if (strcmp(name, pw->pw_name) == 0) {
		*result = pw;
		return NS_SUCCESS;
	} else
		return NS_NOTFOUND;
}

#endif /* YP */


#ifdef _PASSWD_COMPAT
		/*
		 *	compat methods
		 */

	/* state shared between compat methods */

struct compat_state {
	int		 stayopen;	/* see getpassent(3) */
	DB		*db;		/* passwd DB */
	int		 keynum;	/* key counter, -1 if no more */
	enum {				/* current compat mode */
		COMPAT_NOTOKEN = 0,	/*  no compat token present */
		COMPAT_NONE,		/*  parsing normal pwd.db line */
		COMPAT_FULL,		/*  parsing `+' entries */
		COMPAT_USER,		/*  parsing `+name' entries */
		COMPAT_NETGROUP		/*  parsing `+@netgroup' entries */
	}		 mode;
	char		*user;		/* COMPAT_USER "+name" */
	DB		*exclude;	/* compat exclude DB */
	struct passwd	 proto;		/* proto passwd entry */
	char		 protobuf[_GETPW_R_SIZE_MAX];
					/* buffer for proto ptrs */
	int		 protoflags;	/* proto passwd flags */
	int		 version;
};

static struct compat_state	_compat_state;
					/* storage for non _r functions */
static struct passwd		_compat_passwd;
static char			_compat_passwdbuf[_GETPW_R_SIZE_MAX];

static int
_compat_start(struct compat_state *state)
{
	int	rv;

	_DIAGASSERT(state != NULL);

	state->keynum = 0;
	if (state->db == NULL) {		/* not open yet */
		DBT	key, data;
		DBT	pkey, pdata;
		char	bf[MAXLOGNAME];

		rv = _pw_opendb(&state->db, &state->version);
		if (rv != NS_SUCCESS)
			return rv;

		state->mode = COMPAT_NOTOKEN;

		/*
		 *	Determine if the "compat" token is present in pwd.db;
		 *	either "__YP!" or PW_KEYBYNAME+"+".
		 *	Only works if pwd_mkdb installs the token.
		 */
		key.data = (u_char *)__UNCONST(__yp_token);
		key.size = strlen(__yp_token);

		bf[0] = _PW_KEYBYNAME;	 /* Pre-token database support. */
		bf[1] = '+';
		pkey.data = (u_char *)bf;
		pkey.size = 2;

		if ((state->db->get)(state->db, &key, &data, 0) == 0
		    || (state->db->get)(state->db, &pkey, &pdata, 0) == 0)
			state->mode = COMPAT_NONE;
	}
	return NS_SUCCESS;
}

static int
_compat_end(struct compat_state *state)
{

	_DIAGASSERT(state != NULL);

	state->keynum = 0;
	if (state->db) {
		(void)(state->db->close)(state->db);
		state->db = NULL;
	}
	state->mode = COMPAT_NOTOKEN;
	if (state->user)
		free(state->user);
	state->user = NULL;
	if (state->exclude != NULL)
		(void)(state->exclude->close)(state->exclude);
	state->exclude = NULL;
	state->proto.pw_name = NULL;
	state->protoflags = 0;
	return NS_SUCCESS;
}

/*
 * _compat_add_exclude
 *	add the name to the exclude list in state->exclude.
 */
static int
_compat_add_exclude(struct compat_state *state, const char *name)
{
	DBT	key, data;

	_DIAGASSERT(state != NULL);
	_DIAGASSERT(name != NULL);

				/* initialize the exclusion table if needed */
	if (state->exclude == NULL) {
		state->exclude = dbopen(NULL, O_RDWR, 600, DB_HASH, NULL);
		if (state->exclude == NULL)
			return 0;
	}

	key.size = strlen(name);			/* set up the key */
	key.data = (u_char *)__UNCONST(name);

	data.data = NULL;				/* data is nothing */
	data.size = 0;

							/* store it */
	if ((state->exclude->put)(state->exclude, &key, &data, 0) == -1)
		return 0;

	return 1;
}

/*
 * _compat_is_excluded
 *	test if a name is on the compat mode exclude list
 */
static int
_compat_is_excluded(struct compat_state *state, const char *name)
{
	DBT	key, data;

	_DIAGASSERT(state != NULL);
	_DIAGASSERT(name != NULL);

	if (state->exclude == NULL)
		return 0;	/* nothing excluded */

	key.size = strlen(name);			/* set up the key */
	key.data = (u_char *)__UNCONST(name);

	if ((state->exclude->get)(state->exclude, &key, &data, 0) == 0)
		return 1;				/* is excluded */

	return 0;
}


/*
 * _passwdcompat_bad
 *	log an error if "files" or "compat" is specified in
 *	passwd_compat database
 */
/*ARGSUSED*/
static int
_passwdcompat_bad(void *nsrv, void *nscb, va_list ap)
{
	static int warned;

	_DIAGASSERT(nsrv != NULL);
	_DIAGASSERT(nscb != NULL);

	if (!warned) {
		syslog(LOG_ERR,
			"nsswitch.conf passwd_compat database can't use '%s'",
			(char *)nscb);
	}
	warned = 1;
	return NS_UNAVAIL;
}

/*
 * _passwdcompat_setpassent
 *	Call setpassent for all passwd_compat sources.
 */
static int
_passwdcompat_setpassent(int stayopen)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_passwdcompat_bad, "files")
		NS_DNS_CB(_dns_setpassent, NULL)
		NS_NIS_CB(_nis_setpassent, NULL)
		NS_COMPAT_CB(_passwdcompat_bad, "compat")
		NS_NULL_CB
	};

	int	rv, result;

	rv = nsdispatch(NULL, dtab, NSDB_PASSWD_COMPAT, "setpassent",
	    __nsdefaultnis_forceall, &result, stayopen);
	return rv;
}

/*
 * _passwdcompat_endpwent
 *	Call endpwent for all passwd_compat sources.
 */
static int
_passwdcompat_endpwent(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_passwdcompat_bad, "files")
		NS_DNS_CB(_dns_endpwent, NULL)
		NS_NIS_CB(_nis_endpwent, NULL)
		NS_COMPAT_CB(_passwdcompat_bad, "compat")
		NS_NULL_CB
	};

	return nsdispatch(NULL, dtab, NSDB_PASSWD_COMPAT, "endpwent",
	    __nsdefaultnis_forceall);
}

/*
 * _passwdcompat_pwscan
 *	When a name lookup in compat mode is required (e.g., `+name', or a
 *	name in `+@netgroup'), look it up in the 'passwd_compat' nsswitch
 *	database.
 *	Fail if passwd_compat contains files or compat.
 */
static int
_passwdcompat_pwscan(struct passwd *pw, char *buffer, size_t buflen,
	int search, const char *name, uid_t uid)
{
	static const ns_dtab compatentdtab[] = {
		NS_FILES_CB(_passwdcompat_bad, "files")
		NS_DNS_CB(_dns_getpwent_r, NULL)
		NS_NIS_CB(_nis_getpwent_r, NULL)
		NS_COMPAT_CB(_passwdcompat_bad, "compat")
		NS_NULL_CB
	};
	static const ns_dtab compatuiddtab[] = {
		NS_FILES_CB(_passwdcompat_bad, "files")
		NS_DNS_CB(_dns_getpwuid_r, NULL)
		NS_NIS_CB(_nis_getpwuid_r, NULL)
		NS_COMPAT_CB(_passwdcompat_bad, "compat")
		NS_NULL_CB
	};
	static const ns_dtab compatnamdtab[] = {
		NS_FILES_CB(_passwdcompat_bad, "files")
		NS_DNS_CB(_dns_getpwnam_r, NULL)
		NS_NIS_CB(_nis_getpwnam_r, NULL)
		NS_COMPAT_CB(_passwdcompat_bad, "compat")
		NS_NULL_CB
	};

	int		rv, crv;
	struct passwd	*cpw;

	switch (search) {
	case _PW_KEYBYNUM:
		rv = nsdispatch(NULL, compatentdtab,
		    NSDB_PASSWD_COMPAT, "getpwent_r", __nsdefaultnis,
		    &crv, pw, buffer, buflen, &cpw);
		break;
	case _PW_KEYBYNAME:
		_DIAGASSERT(name != NULL);
		rv = nsdispatch(NULL, compatnamdtab,
		    NSDB_PASSWD_COMPAT, "getpwnam_r", __nsdefaultnis,
		    &crv, name, pw, buffer, buflen, &cpw);
		break;
	case _PW_KEYBYUID:
		rv = nsdispatch(NULL, compatuiddtab,
		    NSDB_PASSWD_COMPAT, "getpwuid_r", __nsdefaultnis,
		    &crv, uid, pw, buffer, buflen, &cpw);
		break;
	default:
		abort();
		/*NOTREACHED*/
	}
	return rv;
}

/*
 * _compat_pwscan
 *	Search state->db for the next desired entry.
 *	If search is _PW_KEYBYNUM, look for state->keynum.
 *	If search is _PW_KEYBYNAME, look for name.
 *	If search is _PW_KEYBYUID, look for uid.
 *	Sets *retval to the errno if the result is not NS_SUCCESS
 *	or NS_NOTFOUND.
 */
static int
_compat_pwscan(int *retval, struct passwd *pw, char *buffer, size_t buflen,
	struct compat_state *state, int search, const char *name, uid_t uid)
{
	DBT		 key;
	int		 rv, r, pwflags;
	const char	*user, *host, *dom;
	const void	*from;
	size_t		 fromlen;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(state != NULL);
	/* name may be NULL */

	*retval = 0;

	if (state->db == NULL) {
		rv = _compat_start(state);
		if (rv != NS_SUCCESS)
			return rv;
	}
	if (buflen <= 1) {			/* buffer too small */
		*retval = ERANGE;
		return NS_UNAVAIL;
	}

	for (;;) {				/* loop over pwd.db */
		rv = NS_NOTFOUND;
		if (state->mode != COMPAT_NOTOKEN &&
		    state->mode != COMPAT_NONE) {
						/* doing a compat lookup */
			struct passwd	cpw;
			char		cbuf[_GETPW_R_SIZE_MAX];

			switch (state->mode) {

			case COMPAT_FULL:
					/* get next user or lookup by key */
				rv = _passwdcompat_pwscan(&cpw,
				    cbuf, sizeof(cbuf), search, name, uid);
				if (rv != NS_SUCCESS)
					state->mode = COMPAT_NONE;
				break;

			case COMPAT_NETGROUP:
/* XXXREENTRANT: getnetgrent is not thread safe */
					/* get next user from netgroup */
				r = getnetgrent(&host, &user, &dom);
				if (r == 0) {	/* end of group */
					endnetgrent();
					state->mode = COMPAT_NONE;
					break;
				}
				if (!user || !*user)
					break;
				rv = _passwdcompat_pwscan(&cpw,
				    cbuf, sizeof(cbuf),
				    _PW_KEYBYNAME, user, 0);
				break;

			case COMPAT_USER:
					/* get specific user */
				if (state->user == NULL) {
					state->mode = COMPAT_NONE;
					break;
				}
				rv = _passwdcompat_pwscan(&cpw,
				    cbuf, sizeof(cbuf),
				    _PW_KEYBYNAME, state->user, 0);
				free(state->user);
				state->user = NULL;
				state->mode = COMPAT_NONE;
				break;

			case COMPAT_NOTOKEN:
			case COMPAT_NONE:
				abort();

			}
			if (rv != NS_SUCCESS)	/* if not matched, next loop */
				continue;

				/* copy cpw to pw, applying prototype */
			if (! _pw_copy(&cpw, pw, buffer, buflen,
			    &state->proto, state->protoflags)) {
				rv = NS_UNAVAIL;
				break;
			}

			if (_compat_is_excluded(state, pw->pw_name))
				continue;	/* excluded; next loop */

			if ((search == _PW_KEYBYNAME
					&& strcmp(pw->pw_name, name) != 0)
			    || (search == _PW_KEYBYUID && pw->pw_uid != uid)) {
				continue;	/* not specific; next loop */
			}

			break;			/* exit loop if found */
		} else {			/* not a compat line */
			state->proto.pw_name = NULL;
						/* clear prototype */
		}

		if (state->mode == COMPAT_NOTOKEN) {
				/* no compat token; do direct lookup */
			switch (search) {
			case _PW_KEYBYNUM:
				if (state->keynum == -1)  /* no more records */
					return NS_NOTFOUND;
				state->keynum++;
				from = &state->keynum;
				fromlen = sizeof(state->keynum);
				break;
			case _PW_KEYBYNAME:
				from = name;
				fromlen = strlen(name);
				break;
			case _PW_KEYBYUID:
				from = &uid;
				fromlen = sizeof(uid);
				break;
			default:
				abort();
			}
			buffer[0] = search;
		} else {
				/* compat token; do line by line */
			if (state->keynum == -1)  /* no more records */
				return NS_NOTFOUND;
			state->keynum++;
			from = &state->keynum;
			fromlen = sizeof(state->keynum);
			buffer[0] = _PW_KEYBYNUM;
		}

		if (buflen <= fromlen) {		/* buffer too small */
			*retval = ERANGE;
			return NS_UNAVAIL;
		}
		memmove(buffer + 1, from, fromlen);	/* setup key */
		key.size = fromlen + 1;
		key.data = (u_char *)buffer;

		rv = _pw_getkey(state->db, &key, pw, buffer, buflen, &pwflags,
		    state->version);
		if (rv != NS_SUCCESS)		/* stop on error */
			break;

		if (state->mode == COMPAT_NOTOKEN)
			break;			/* stop if no compat token */

		if (pw->pw_name[0] == '+') {
						/* compat inclusion */
			switch(pw->pw_name[1]) {
			case '\0':		/* `+' */
				state->mode = COMPAT_FULL;
						/* reset passwd_compat search */
/* XXXREENTRANT: setpassent is not thread safe ? */
				(void) _passwdcompat_setpassent(_compat_state.stayopen);
				break;
			case '@':		/* `+@netgroup' */
				state->mode = COMPAT_NETGROUP;
						/* reset netgroup search */
/* XXXREENTRANT: setnetgrent is not thread safe */
				setnetgrent(pw->pw_name + 2);
				break;
			default:		/* `+name' */
				state->mode = COMPAT_USER;
				if (state->user)
					free(state->user);
				state->user = strdup(pw->pw_name + 1);
				break;
			}
						/* save the prototype */
			state->protoflags = pwflags;
			if (! _pw_copy(pw, &state->proto, state->protobuf,
			    sizeof(state->protobuf), NULL, 0)) {
				rv = NS_UNAVAIL;
				break;
			}
			continue;		/* loop again after inclusion */
		} else if (pw->pw_name[0] == '-') {
						/* compat exclusion */
			rv = NS_SUCCESS;
			switch(pw->pw_name[1]) {
			case '\0':		/* `-' */
				break;
			case '@':		/* `-@netgroup' */
/* XXXREENTRANT: {set,get,end}netgrent is not thread safe */
				setnetgrent(pw->pw_name + 2);
				while (getnetgrent(&host, &user, &dom)) {
					if (!user || !*user)
						continue;
					if (! _compat_add_exclude(state,user)) {
						rv = NS_UNAVAIL;
						break;
					}
				}
				endnetgrent();
				break;
			default:		/* `-name' */
				if (! _compat_add_exclude(state,
				    pw->pw_name + 1)) {
					rv = NS_UNAVAIL;
				}
				break;
			}
			if (rv != NS_SUCCESS)	/* exclusion failure */
				break;
			continue;		/* loop again after exclusion */
		}
		if (search == _PW_KEYBYNUM ||
		    (search == _PW_KEYBYUID && pw->pw_uid == uid) ||
		    (search == _PW_KEYBYNAME && strcmp(pw->pw_name, name) == 0))
			break;			/* token mode match found */
	}

	if (rv == NS_NOTFOUND &&
	    (search == _PW_KEYBYNUM || state->mode != COMPAT_NOTOKEN))
		state->keynum = -1;		/* flag `no more records' */

	if (rv == NS_SUCCESS) {
		if ((search == _PW_KEYBYNAME && strcmp(pw->pw_name, name) != 0)
		    || (search == _PW_KEYBYUID && pw->pw_uid != uid))
			rv = NS_NOTFOUND;
	}

	if (rv != NS_SUCCESS && rv != NS_NOTFOUND)
		*retval = errno;
	return rv;
}

/*ARGSUSED*/
static int
_compat_setpwent(void *nsrv, void *nscb, va_list ap)
{

					/* force passwd_compat setpwent() */
	(void) _passwdcompat_setpassent(0);

					/* reset state, keep db open */
	_compat_state.stayopen = 0;
	return _compat_start(&_compat_state);
}

/*ARGSUSED*/
static int
_compat_setpassent(void *nsrv, void *nscb, va_list ap)
{
	int	*retval		= va_arg(ap, int *);
	int	 stayopen	= va_arg(ap, int);

	int	rv;

					/* force passwd_compat setpassent() */
	(void) _passwdcompat_setpassent(stayopen);

	_compat_state.stayopen = stayopen;
	rv = _compat_start(&_compat_state);
	*retval = (rv == NS_SUCCESS);
	return rv;
}

/*ARGSUSED*/
static int
_compat_endpwent(void *nsrv, void *nscb, va_list ap)
{

					/* force passwd_compat endpwent() */
	(void) _passwdcompat_endpwent();

					/* reset state, close db */
	_compat_state.stayopen = 0;
	return _compat_end(&_compat_state);
}


/*ARGSUSED*/
static int
_compat_getpwent(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _compat_pwscan(&rerror, &_compat_passwd,
	    _compat_passwdbuf, sizeof(_compat_passwdbuf),
	    &_compat_state, _PW_KEYBYNUM, NULL, 0);
	if (rv == NS_SUCCESS)
		*retval = &_compat_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getpwent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	int		rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	rv = _compat_pwscan(retval, pw, buffer, buflen, &_compat_state,
	    _PW_KEYBYNUM, NULL, 0);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*result = NULL;
	return rv;
}


/*ARGSUSED*/
static int
_compat_getpwnam(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _compat_start(&_compat_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = _compat_pwscan(&rerror, &_compat_passwd,
	    _compat_passwdbuf, sizeof(_compat_passwdbuf),
	    &_compat_state, _PW_KEYBYNAME, name, 0);
	if (!_compat_state.stayopen)
		_compat_end(&_compat_state);
	if (rv == NS_SUCCESS)
		*retval = &_compat_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getpwnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct compat_state	state;
	int		rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = _compat_pwscan(retval, pw, buffer, buflen, &state,
	    _PW_KEYBYNAME, name, 0);
	_compat_end(&state);
	if (rv == NS_SUCCESS)
		*result = pw;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getpwuid(void *nsrv, void *nscb, va_list ap)
{
	struct passwd	**retval = va_arg(ap, struct passwd **);
	uid_t		 uid	= va_arg(ap, uid_t);

	int	rv, rerror;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	rv = _compat_start(&_compat_state);
	if (rv != NS_SUCCESS)
		return rv;
	rv = _compat_pwscan(&rerror, &_compat_passwd,
	    _compat_passwdbuf, sizeof(_compat_passwdbuf),
	    &_compat_state, _PW_KEYBYUID, NULL, uid);
	if (!_compat_state.stayopen)
		_compat_end(&_compat_state);
	if (rv == NS_SUCCESS)
		*retval = &_compat_passwd;
	return rv;
}

/*ARGSUSED*/
static int
_compat_getpwuid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval	= va_arg(ap, int *);
	uid_t		 uid	= va_arg(ap, uid_t);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	struct compat_state	state;
	int		rv;

	_DIAGASSERT(retval != NULL);
	_DIAGASSERT(pw != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	memset(&state, 0, sizeof(state));
	rv = _compat_pwscan(retval, pw, buffer, buflen, &state,
	    _PW_KEYBYUID, NULL, uid);
	_compat_end(&state);
	if (rv == NS_SUCCESS)
		*result = pw;
	return rv;
}

#endif /* _PASSWD_COMPAT */


		/*
		 *	public functions
		 */

struct passwd *
getpwent(void)
{
	int		r;
	struct passwd	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getpwent, NULL)
		NS_DNS_CB(_dns_getpwent, NULL)
		NS_NIS_CB(_nis_getpwent, NULL)
		NS_COMPAT_CB(_compat_getpwent, NULL)
		NS_NULL_CB
	};

	mutex_lock(&_pwmutex);
	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwent", __nsdefaultcompat,
	    &retval);
	mutex_unlock(&_pwmutex);
	return (r == NS_SUCCESS) ? retval : NULL;
}

int
getpwent_r(struct passwd *pwd, char *buffer, size_t buflen,
    struct passwd **result)
{
	int	r, retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getpwent_r, NULL)
		NS_DNS_CB(_dns_getpwent_r, NULL)
		NS_NIS_CB(_nis_getpwent_r, NULL)
		NS_COMPAT_CB(_compat_getpwent_r, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(pwd != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	retval = 0;
	mutex_lock(&_pwmutex);
	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwent_r", __nsdefaultcompat,
	    &retval, pwd, buffer, buflen, result);
	mutex_unlock(&_pwmutex);
	switch (r) {
	case NS_SUCCESS:
	case NS_NOTFOUND:
		return 0;
	default:
		return retval;
	}
}


struct passwd *
getpwnam(const char *name)
{
	int		rv;
	struct passwd	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getpwnam, NULL)
		NS_DNS_CB(_dns_getpwnam, NULL)
		NS_NIS_CB(_nis_getpwnam, NULL)
		NS_COMPAT_CB(_compat_getpwnam, NULL)
		NS_NULL_CB
	};

	mutex_lock(&_pwmutex);
	rv = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwnam", __nsdefaultcompat,
	    &retval, name);
	mutex_unlock(&_pwmutex);
	return (rv == NS_SUCCESS) ? retval : NULL;
}

int
getpwnam_r(const char *name, struct passwd *pwd, char *buffer, size_t buflen,
	struct passwd **result)
{
	int	r, retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getpwnam_r, NULL)
		NS_DNS_CB(_dns_getpwnam_r, NULL)
		NS_NIS_CB(_nis_getpwnam_r, NULL)
		NS_COMPAT_CB(_compat_getpwnam_r, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(name != NULL);
	_DIAGASSERT(pwd != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	retval = 0;
	mutex_lock(&_pwmutex);
	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwnam_r", __nsdefaultcompat,
	    &retval, name, pwd, buffer, buflen, result);
	mutex_unlock(&_pwmutex);
	switch (r) {
	case NS_SUCCESS:
	case NS_NOTFOUND:
		return 0;
	default:
		return retval;
	}
}

struct passwd *
getpwuid(uid_t uid)
{
	int		rv;
	struct passwd	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getpwuid, NULL)
		NS_DNS_CB(_dns_getpwuid, NULL)
		NS_NIS_CB(_nis_getpwuid, NULL)
		NS_COMPAT_CB(_compat_getpwuid, NULL)
		NS_NULL_CB
	};

	mutex_lock(&_pwmutex);
	rv = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwuid", __nsdefaultcompat,
	    &retval, uid);
	mutex_unlock(&_pwmutex);
	return (rv == NS_SUCCESS) ? retval : NULL;
}

int
getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t buflen,
	struct passwd **result)
{
	int	r, retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getpwuid_r, NULL)
		NS_DNS_CB(_dns_getpwuid_r, NULL)
		NS_NIS_CB(_nis_getpwuid_r, NULL)
		NS_COMPAT_CB(_compat_getpwuid_r, NULL)
		NS_NULL_CB
	};

	_DIAGASSERT(pwd != NULL);
	_DIAGASSERT(buffer != NULL);
	_DIAGASSERT(result != NULL);

	*result = NULL;
	retval = 0;
	mutex_lock(&_pwmutex);
	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwuid_r", __nsdefaultcompat,
	    &retval, uid, pwd, buffer, buflen, result);
	mutex_unlock(&_pwmutex);
	switch (r) {
	case NS_SUCCESS:
	case NS_NOTFOUND:
		return 0;
	default:
		return retval;
	}
}

void
endpwent(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_endpwent, NULL)
		NS_DNS_CB(_dns_endpwent, NULL)
		NS_NIS_CB(_nis_endpwent, NULL)
		NS_COMPAT_CB(_compat_endpwent, NULL)
		NS_NULL_CB
	};

	mutex_lock(&_pwmutex);
					/* force all endpwent() methods */
	(void) nsdispatch(NULL, dtab, NSDB_PASSWD, "endpwent",
	    __nsdefaultcompat_forceall);
	mutex_unlock(&_pwmutex);
}

/*ARGSUSED*/
int
setpassent(int stayopen)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_setpassent, NULL)
		NS_DNS_CB(_dns_setpassent, NULL)
		NS_NIS_CB(_nis_setpassent, NULL)
		NS_COMPAT_CB(_compat_setpassent, NULL)
		NS_NULL_CB
	};
	int	rv, retval;

	mutex_lock(&_pwmutex);
					/* force all setpassent() methods */
	rv = nsdispatch(NULL, dtab, NSDB_PASSWD, "setpassent",
	    __nsdefaultcompat_forceall, &retval, stayopen);
	mutex_unlock(&_pwmutex);
	return (rv == NS_SUCCESS) ? retval : 0;
}

void
setpwent(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_setpwent, NULL)
		NS_DNS_CB(_dns_setpwent, NULL)
		NS_NIS_CB(_nis_setpwent, NULL)
		NS_COMPAT_CB(_compat_setpwent, NULL)
		NS_NULL_CB
	};

	mutex_lock(&_pwmutex);
					/* force all setpwent() methods */
	(void) nsdispatch(NULL, dtab, NSDB_PASSWD, "setpwent",
	    __nsdefaultcompat_forceall);
	mutex_unlock(&_pwmutex);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getpwent, __getpwent50)
__weak_alias(getpwent_r, __getpwent_r50)
__weak_alias(getpwnam, __getpwnam50)
__weak_alias(getpwnam_r, __getpwnam_r50)
__weak_alias(getpwuid, __getpwuid50)
__weak_alias(getpwuid_r, __getpwuid_r50)
#endif
