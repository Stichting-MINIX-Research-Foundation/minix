/*	$NetBSD: getusershell.c,v 1.29 2012/03/13 21:13:36 christos Exp $	*/

/*-
 * Copyright (c) 1999, 2005 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1985, 1993
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
static char sccsid[] = "@(#)getusershell.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: getusershell.c,v 1.29 2012/03/13 21:13:36 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include "reentrant.h"

#include <sys/param.h>
#include <sys/file.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <nsswitch.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HESIOD
#include <hesiod.h>
#endif
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#endif

#ifdef __weak_alias
__weak_alias(endusershell,_endusershell)
__weak_alias(getusershell,_getusershell)
__weak_alias(setusershell,_setusershell)
#endif

/*
 * Local shells should NOT be added here.
 * They should be added in /etc/shells.
 */
static const char *const okshells[] = { _PATH_BSHELL, _PATH_CSHELL, NULL };

#ifdef _REENTRANT
static mutex_t __shellmutex = MUTEX_INITIALIZER;
#endif

static char		  curshell[MAXPATHLEN + 2];

static const char *const *curokshell = okshells;
static int		  shellsfound = 0;

		/*
		 *	files methods
		 */

	/* state shared between files methods */
struct files_state {
	FILE	*fp;
};

static struct files_state _files_state;


static int
_files_start(struct files_state *state)
{

	_DIAGASSERT(state != NULL);

	if (state->fp == NULL) {
		state->fp = fopen(_PATH_SHELLS, "re");
		if (state->fp == NULL)
			return NS_UNAVAIL;
	} else {
		rewind(state->fp);
	}
	return NS_SUCCESS;
}

static int
_files_end(struct files_state *state)
{

	_DIAGASSERT(state != NULL);

	if (state->fp) {
		(void) fclose(state->fp);
		state->fp = NULL;
	}
	return NS_SUCCESS;
}

/*ARGSUSED*/
static int
_files_setusershell(void *nsrv, void *nscb, va_list ap)
{

	return _files_start(&_files_state);
}

/*ARGSUSED*/
static int
_files_endusershell(void *nsrv, void *nscb, va_list ap)
{

	return _files_end(&_files_state);
}

/*ARGSUSED*/
static int
_files_getusershell(void *nsrv, void *nscb, va_list ap)
{
	char	**retval = va_arg(ap, char **);

	char	*sp, *cp;
	int	 rv;

	_DIAGASSERT(retval != NULL);

	*retval = NULL;
	if (_files_state.fp == NULL) {	/* only start if file not open yet */
		rv = _files_start(&_files_state);
		if (rv != NS_SUCCESS)
			return rv;
	}

	while (fgets(curshell, (int)sizeof(curshell) - 1, _files_state.fp)
	    != NULL) {
		sp = cp = curshell;
		while (*cp != '#' && *cp != '/' && *cp != '\0')
			cp++;
		if (*cp == '#' || *cp == '\0')
			continue;
		sp = cp;
		while (!isspace((unsigned char) *cp) && *cp != '#'
		    && *cp != '\0')
			cp++;
		*cp++ = '\0';
		*retval = sp;
		return NS_SUCCESS;
	}

	return NS_NOTFOUND;
}


#ifdef HESIOD
		/*
		 *	dns methods
		 */

	/* state shared between dns methods */
struct dns_state {
	void	*context;		/* Hesiod context */
	int	 num;			/* shell index, -1 if no more */
};

static struct dns_state		_dns_state;

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

/*ARGSUSED*/
static int
_dns_setusershell(void *nsrv, void *nscb, va_list ap)
{

	return _dns_start(&_dns_state);
}

/*ARGSUSED*/
static int
_dns_endusershell(void *nsrv, void *nscb, va_list ap)
{

	return _dns_end(&_dns_state);
}

/*ARGSUSED*/
static int
_dns_getusershell(void *nsrv, void *nscb, va_list ap)
{
	char	**retval = va_arg(ap, char **);

	char	  shellname[] = "shells-NNNNNNNNNN";
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

	hp = NULL;
	rv = NS_NOTFOUND;

							/* find shells-NNN */
	snprintf(shellname, sizeof(shellname), "shells-%d", _dns_state.num);
	_dns_state.num++;

	hp = hesiod_resolve(_dns_state.context, shellname, "shells");
	if (hp == NULL) {
		if (errno == ENOENT)
			rv = NS_NOTFOUND;
		else
			rv = NS_UNAVAIL;
	} else {
		if ((ep = strchr(hp[0], '\n')) != NULL)
			*ep = '\0';			/* clear trailing \n */
						/* only use first result */
		strlcpy(curshell, hp[0], sizeof(curshell));
		*retval = curshell;
		rv = NS_SUCCESS;
	}

	if (hp)
		hesiod_free_list(_dns_state.context, hp);
	if (rv != NS_SUCCESS)
		_dns_state.num = -1;		/* any failure halts search */
	return rv;
}

#endif /* HESIOD */


#ifdef YP
		/*
		 *	nis methods
		 */
	/* state shared between nis methods */
struct nis_state {
	char		*domain;	/* NIS domain */
	int		 done;		/* non-zero if search exhausted */
	char		*current;	/* current first/next match */
	int		 currentlen;	/* length of _nis_current */
};

static struct nis_state		_nis_state;

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
	return NS_SUCCESS;
}

/*ARGSUSED*/
static int
_nis_setusershell(void *nsrv, void *nscb, va_list ap)
{

	return _nis_start(&_nis_state);
}

/*ARGSUSED*/
static int
_nis_endusershell(void *nsrv, void *nscb, va_list ap)
{

	return _nis_end(&_nis_state);
}

/*ARGSUSED*/
static int
_nis_getusershell(void *nsrv, void *nscb, va_list ap)
{
	char	**retval = va_arg(ap, char **);

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

	key = NULL;
	data = NULL;
	rv = NS_NOTFOUND;

	if (_nis_state.current) {			/* already searching */
		nisr = yp_next(_nis_state.domain, "shells",
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
			rv = NS_NOTFOUND;
			goto nisent_out;
		default:
			rv = NS_UNAVAIL;
			goto nisent_out;
		}
	} else {					/* new search */
		if (yp_first(_nis_state.domain, "shells",
		    &_nis_state.current, &_nis_state.currentlen,
		    &data, &datalen)) {
			rv = NS_UNAVAIL;
			goto nisent_out;
		}
	}

	data[datalen] = '\0';				/* clear trailing \n */
	strlcpy(curshell, data, sizeof(curshell));
	*retval = curshell;
	rv = NS_SUCCESS;

 nisent_out:
	if (key)
		free(key);
	if (data)
		free(data);
	if (rv != NS_SUCCESS)			/* any failure halts search */
		_nis_state.done = 1;
	return rv;
}

#endif /* YP */


		/*
		 *	public functions
		 */

void
endusershell(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_endusershell, NULL)
		NS_DNS_CB(_dns_endusershell, NULL)
		NS_NIS_CB(_nis_endusershell, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__shellmutex);

	curokshell = okshells;		/* reset okshells fallback state */
	shellsfound = 0;

					/* force all endusershell() methods */
	(void) nsdispatch(NULL, dtab, NSDB_SHELLS, "endusershell",
	    __nsdefaultfiles_forceall);
	mutex_unlock(&__shellmutex);
}

__aconst char *
getusershell(void)
{
	int		 rv;
	__aconst char	*retval;

	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_getusershell, NULL)
		NS_DNS_CB(_dns_getusershell, NULL)
		NS_NIS_CB(_nis_getusershell, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__shellmutex);

	retval = NULL;
	do {
		rv = nsdispatch(NULL, dtab, NSDB_SHELLS, "getusershell",
		    __nsdefaultsrc, &retval);
				/* loop until failure or non-blank result */
	} while (rv == NS_SUCCESS && retval[0] == '\0');

	if (rv == NS_SUCCESS) {
		shellsfound++;
	} else if (shellsfound == 0) {	/* no shells; fall back to okshells */
		if (curokshell != NULL) {
			retval = __UNCONST(*curokshell);
			curokshell++;
			rv = NS_SUCCESS;
		}
	}

	mutex_unlock(&__shellmutex);
	return (rv == NS_SUCCESS) ? retval : NULL;
}

void
setusershell(void)
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_files_setusershell, NULL)
		NS_DNS_CB(_dns_setusershell, NULL)
		NS_NIS_CB(_nis_setusershell, NULL)
		NS_NULL_CB
	};

	mutex_lock(&__shellmutex);

	curokshell = okshells;		/* reset okshells fallback state */
	shellsfound = 0;

					/* force all setusershell() methods */
	(void) nsdispatch(NULL, dtab, NSDB_SHELLS, "setusershell",
	    __nsdefaultfiles_forceall);
	mutex_unlock(&__shellmutex);
}
