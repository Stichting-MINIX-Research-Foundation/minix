/*	$NetBSD: yperr_string.c,v 1.9 2015/06/17 00:15:26 christos Exp $	 */

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: yperr_string.c,v 1.9 2015/06/17 00:15:26 christos Exp $");
#endif

#include "namespace.h"
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#ifdef __weak_alias
__weak_alias(yperr_string,_yperr_string)
#endif

char *
yperr_string(int incode)
{
	static char     err[80];

	switch (incode) {
	case 0:
		return __UNCONST("Success");
	case YPERR_BADARGS:
		return __UNCONST("Request arguments bad");
	case YPERR_RPC:
		return __UNCONST("RPC failure");
	case YPERR_DOMAIN:
		return __UNCONST(
		    "Can't bind to server which serves this domain");
	case YPERR_MAP:
		return __UNCONST("No such map in server's domain");
	case YPERR_KEY:
		return __UNCONST("No such key in map");
	case YPERR_YPERR:
		return __UNCONST("YP server error");
	case YPERR_RESRC:
		return __UNCONST("Local resource allocation failure");
	case YPERR_NOMORE:
		return __UNCONST("No more records in map database");
	case YPERR_PMAP:
		return __UNCONST("Can't communicate with portmapper");
	case YPERR_YPBIND:
		return __UNCONST("Can't communicate with ypbind");
	case YPERR_YPSERV:
		return __UNCONST("Can't communicate with ypserv");
	case YPERR_NODOM:
		return __UNCONST("Local domain name not set");
	case YPERR_BADDB:
		return __UNCONST("Server data base is bad");
	case YPERR_VERS:
		return __UNCONST(
		    "YP server version mismatch - server can't supply service."
		    );
	case YPERR_ACCESS:
		return __UNCONST("Access violation");
	case YPERR_BUSY:
		return __UNCONST("Database is busy");
	}
	(void) snprintf(err, sizeof(err), "YP unknown error %d", incode);
	return err;
}
