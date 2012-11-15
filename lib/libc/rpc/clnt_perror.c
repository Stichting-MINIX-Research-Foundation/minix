/*	$NetBSD: clnt_perror.c,v 1.29 2012/03/20 17:14:50 matt Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char *sccsid = "@(#)clnt_perror.c 1.15 87/10/07 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)clnt_perror.c	2.1 88/07/29 4.0 RPCSRC";
#else
__RCSID("$NetBSD: clnt_perror.c,v 1.29 2012/03/20 17:14:50 matt Exp $");
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
