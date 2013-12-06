/*	$NetBSD: clnt_perror.c,v 1.30 2013/03/11 20:19:29 tron Exp $	*/

/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the "Oracle America, Inc." nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)clnt_perror.c 1.15 87/10/07 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)clnt_perror.c	2.1 88/07/29 4.0 RPCSRC";
#else
__RCSID("$NetBSD: clnt_perror.c,v 1.30 2013/03/11 20:19:29 tron Exp $");
#endif
#endif

/*
 * clnt_perror.c
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 */
#include "namespace.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

#ifdef __weak_alias
__weak_alias(clnt_pcreateerror,_clnt_pcreateerror)
__weak_alias(clnt_perrno,_clnt_perrno)
__weak_alias(clnt_perror,_clnt_perror)
__weak_alias(clnt_spcreateerror,_clnt_spcreateerror)
__weak_alias(clnt_sperrno,_clnt_sperrno)
__weak_alias(clnt_sperror,_clnt_sperror)
#endif

static char *buf;
static size_t buflen;

static char *_buf(void);
static char *auth_errmsg(enum auth_stat);

static char *
_buf(void)
{

	buflen = 256;
	if (buf == 0)
		buf = malloc(buflen);
	return (buf);
}

/*
 * Print reply error info
 */
char *
clnt_sperror(CLIENT *rpch, const char *s)
{
	struct rpc_err e;
	char *err;
	char *str;
	char *strstart;
	size_t len, i;

	_DIAGASSERT(rpch != NULL);
	_DIAGASSERT(s != NULL);

	str = _buf(); /* side effect: sets "buflen" */
	if (str == 0)
		return (0);
	len = buflen;
	strstart = str;
	CLNT_GETERR(rpch, &e);

	i = snprintf(str, len, "%s: ", s);  
	str += i;
	len -= i;

	(void)strncpy(str, clnt_sperrno(e.re_status), len - 1);
	i = strlen(str);
	str += i;
	len -= i;

	switch (e.re_status) {
	case RPC_SUCCESS:
	case RPC_CANTENCODEARGS:
	case RPC_CANTDECODERES:
	case RPC_TIMEDOUT:     
	case RPC_PROGUNAVAIL:
	case RPC_PROCUNAVAIL:
	case RPC_CANTDECODEARGS:
	case RPC_SYSTEMERROR:
	case RPC_UNKNOWNHOST:
	case RPC_UNKNOWNPROTO:
	case RPC_PMAPFAILURE:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
		break;

	case RPC_CANTSEND:
	case RPC_CANTRECV:
		i = snprintf(str, len, "; errno = %s", strerror(e.re_errno)); 
		str += i;
		len -= i;
		break;

	case RPC_VERSMISMATCH:
		i = snprintf(str, len, "; low version = %u, high version = %u", 
			e.re_vers.low, e.re_vers.high);
		str += i;
		len -= i;
		break;

	case RPC_AUTHERROR:
		err = auth_errmsg(e.re_why);
		i = snprintf(str, len, "; why = ");
		str += i;
		len -= i;
		if (err != NULL) {
			i = snprintf(str, len, "%s",err);
		} else {
			i = snprintf(str, len,
				"(unknown authentication error - %d)",
				(int) e.re_why);
		}
		str += i;
		len -= i;
		break;

	case RPC_PROGVERSMISMATCH:
		i = snprintf(str, len, "; low version = %u, high version = %u", 
			e.re_vers.low, e.re_vers.high);
		str += i;
		len -= i;
		break;

	default:	/* unknown */
		i = snprintf(str, len, "; s1 = %u, s2 = %u", 
			e.re_lb.s1, e.re_lb.s2);
		str += i;
		len -= i;
		break;
	}
	return(strstart) ;
}

void
clnt_perror(CLIENT *rpch, const char *s)
{

	_DIAGASSERT(rpch != NULL);
	_DIAGASSERT(s != NULL);

	(void) fprintf(stderr, "%s\n", clnt_sperror(rpch,s));
}

static const char *const rpc_errlist[] = {
	[RPC_SUCCESS] =			"RPC: Success",
	[RPC_CANTENCODEARGS] =		"RPC: Can't encode arguments",
	[RPC_CANTDECODERES] =		"RPC: Can't decode result",
	[RPC_CANTSEND] =		"RPC: Unable to send",
	[RPC_CANTRECV] =		"RPC: Unable to receive",
	[RPC_TIMEDOUT] =		"RPC: Timed out",
	[RPC_VERSMISMATCH] =		"RPC: Incompatible versions of RPC",
	[RPC_AUTHERROR] = 		"RPC: Authentication error",
	[RPC_PROGUNAVAIL] =		"RPC: Program unavailable",
	[RPC_PROGVERSMISMATCH] =	"RPC: Program/version mismatch",
	[RPC_PROCUNAVAIL] =		"RPC: Procedure unavailable",
	[RPC_CANTDECODEARGS] =		"RPC: Server can't decode arguments",
	[RPC_SYSTEMERROR] =		"RPC: Remote system error",
	[RPC_UNKNOWNHOST] =		"RPC: Unknown host",
	[RPC_PMAPFAILURE] =		"RPC: Port mapper failure",
	[RPC_PROGNOTREGISTERED] =	"RPC: Program not registered",
	[RPC_FAILED] =			"RPC: Failed (unspecified error)",
	[RPC_UNKNOWNPROTO] =		"RPC: Unknown protocol",
	[RPC_UNKNOWNADDR] =		"RPC: Remote address unknown",
	[RPC_TLIERROR] =		"RPC: Misc error in the TLI library",
	[RPC_NOBROADCAST] =		"RPC: Broadcasting not supported",
	[RPC_N2AXLATEFAILURE] =		"RPC: Name -> addr translation failed",
	[RPC_INPROGRESS] =		"RPC: In progress",
	[RPC_STALERACHANDLE] =		"RPC: Stale handle",
};


/*
 * This interface for use by clntrpc
 */
char *
clnt_sperrno(enum clnt_stat stat)
{
	unsigned int errnum = stat;
	const char *msg;

	msg = NULL;
	if (errnum < (sizeof(rpc_errlist)/sizeof(rpc_errlist[0]))) {
		msg = rpc_errlist[errnum];
	}
	if (msg == NULL) {
		msg = "RPC: (unknown error code)";
	}
	return __UNCONST(msg);
}

void
clnt_perrno(enum clnt_stat num)
{
	(void) fprintf(stderr, "%s\n", clnt_sperrno(num));
}


char *
clnt_spcreateerror(const char *s)
{
	char *str;
	size_t len, i;

	_DIAGASSERT(s != NULL);

	str = _buf(); /* side effect: sets "buflen" */
	if (str == 0)
		return(0);
	len = buflen;
	i = snprintf(str, len, "%s: ", s);
	len -= i;
	(void)strncat(str, clnt_sperrno(rpc_createerr.cf_stat), len - 1);
	switch (rpc_createerr.cf_stat) {
	case RPC_PMAPFAILURE:
		(void) strncat(str, " - ", len - 1);
		(void) strncat(str,
		    clnt_sperrno(rpc_createerr.cf_error.re_status), len - 4);
		break;

	case RPC_SYSTEMERROR:
		(void)strncat(str, " - ", len - 1);
		(void)strncat(str, strerror(rpc_createerr.cf_error.re_errno),
		    len - 4);
		break;

	case RPC_CANTSEND:
	case RPC_CANTDECODERES:
	case RPC_CANTENCODEARGS:
	case RPC_SUCCESS:
	case RPC_UNKNOWNPROTO:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
	case RPC_UNKNOWNHOST:
	case RPC_CANTDECODEARGS:
	case RPC_PROCUNAVAIL:
	case RPC_PROGVERSMISMATCH:
	case RPC_PROGUNAVAIL:
	case RPC_AUTHERROR:
	case RPC_VERSMISMATCH:
	case RPC_TIMEDOUT:
	case RPC_CANTRECV:
	default:
		break;
	}
	return (str);
}

void
clnt_pcreateerror(const char *s)
{

	_DIAGASSERT(s != NULL);

	(void) fprintf(stderr, "%s\n", clnt_spcreateerror(s));
}

static const char *const auth_errlist[] = {
	"Authentication OK",			/* 0 - AUTH_OK */
	"Invalid client credential",		/* 1 - AUTH_BADCRED */
	"Server rejected credential",		/* 2 - AUTH_REJECTEDCRED */
	"Invalid client verifier", 		/* 3 - AUTH_BADVERF */
	"Server rejected verifier", 		/* 4 - AUTH_REJECTEDVERF */
	"Client credential too weak",		/* 5 - AUTH_TOOWEAK */
	"Invalid server verifier",		/* 6 - AUTH_INVALIDRESP */
	"Failed (unspecified error)"		/* 7 - AUTH_FAILED */
};

static char *
auth_errmsg(enum auth_stat stat)
{
	unsigned int errnum = stat;

	if (errnum < __arraycount(auth_errlist))
		return __UNCONST(auth_errlist[errnum]);

	return(NULL);
}
