/*	$NetBSD: ypprot_err.c,v 1.6 2012/06/25 22:32:46 abs Exp $	 */

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
__RCSID("$NetBSD: ypprot_err.c,v 1.6 2012/06/25 22:32:46 abs Exp $");
#endif

#include "namespace.h"
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#ifdef __weak_alias
__weak_alias(ypprot_err,_ypprot_err)
#endif

int
ypprot_err(unsigned int incode)
{
	switch (incode) {
	case YP_TRUE:
		return 0;
	case YP_FALSE:
		return YPERR_YPBIND;
	case YP_NOMORE:
		return YPERR_NOMORE;
	case YP_NOMAP:
		return YPERR_MAP;
	case YP_NODOM:
		return YPERR_NODOM;
	case YP_NOKEY:
		return YPERR_KEY;
	case YP_BADOP:
		return YPERR_YPERR;
	case YP_BADDB:
		return YPERR_BADDB;
	case YP_YPERR:
		return YPERR_YPERR;
	case YP_BADARGS:
		return YPERR_BADARGS;
	case YP_VERS:
		return YPERR_VERS;
	}
	return YPERR_YPERR;
}
