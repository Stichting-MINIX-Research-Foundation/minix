/*	$NetBSD: toisc.c,v 1.1.1.2 2014/07/12 11:58:00 spz Exp $	*/
/* toisc.c

   Convert non-ISC result codes to ISC result codes. */

/*
 * Copyright (c) 2004,2007,2009,2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2001-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: toisc.c,v 1.1.1.2 2014/07/12 11:58:00 spz Exp $");

#include "dhcpd.h"

#include <omapip/omapip_p.h>
#include "arpa/nameser.h"
#include "minires.h"

#include <errno.h>

isc_result_t uerr2isc (int err)
{
	switch (err) {
	      case EPERM:
		return ISC_R_NOPERM;

	      case ENOENT:
		return ISC_R_NOTFOUND;

	      case ESRCH:
		return ISC_R_NOTFOUND;

	      case EIO:
		return ISC_R_IOERROR;

	      case ENXIO:
		return ISC_R_NOTFOUND;

	      case E2BIG:
		return ISC_R_NOSPACE;

	      case ENOEXEC:
		return DHCP_R_FORMERR;

	      case ECHILD:
		return ISC_R_NOTFOUND;

	      case ENOMEM:
		return ISC_R_NOMEMORY;

	      case EACCES:
		return ISC_R_NOPERM;

	      case EFAULT:
		return DHCP_R_INVALIDARG;

	      case EEXIST:
		return ISC_R_EXISTS;

	      case EINVAL:
		return DHCP_R_INVALIDARG;

	      case ENOTTY:
		return DHCP_R_INVALIDARG;

	      case EFBIG:
		return ISC_R_NOSPACE;

	      case ENOSPC:
		return ISC_R_NOSPACE;

	      case EROFS:
		return ISC_R_NOPERM;

	      case EMLINK:
		return ISC_R_NOSPACE;

	      case EPIPE:
		return ISC_R_NOTCONNECTED;

	      case EINPROGRESS:
		return ISC_R_ALREADYRUNNING;

	      case EALREADY:
		return ISC_R_ALREADYRUNNING;

	      case ENOTSOCK:
		return ISC_R_INVALIDFILE;

	      case EDESTADDRREQ:
		return DHCP_R_DESTADDRREQ;

	      case EMSGSIZE:
		return ISC_R_NOSPACE;

	      case EPROTOTYPE:
		return DHCP_R_INVALIDARG;

	      case ENOPROTOOPT:
		return ISC_R_NOTIMPLEMENTED;

	      case EPROTONOSUPPORT:
		return ISC_R_NOTIMPLEMENTED;

	      case ESOCKTNOSUPPORT:
		return ISC_R_NOTIMPLEMENTED;

	      case EOPNOTSUPP:
		return ISC_R_NOTIMPLEMENTED;

	      case EPFNOSUPPORT:
		return ISC_R_NOTIMPLEMENTED;

	      case EAFNOSUPPORT:
		return ISC_R_NOTIMPLEMENTED;

	      case EADDRINUSE:
		return ISC_R_ADDRINUSE;

	      case EADDRNOTAVAIL:
		return ISC_R_ADDRNOTAVAIL;

	      case ENETDOWN:
		return ISC_R_NETDOWN;

	      case ENETUNREACH:
		return ISC_R_NETUNREACH;

	      case ECONNABORTED:
		return ISC_R_TIMEDOUT;

	      case ECONNRESET:
		return DHCP_R_CONNRESET;

	      case ENOBUFS:
		return ISC_R_NOSPACE;

	      case EISCONN:
		return ISC_R_ALREADYRUNNING;

	      case ENOTCONN:
		return ISC_R_NOTCONNECTED;

	      case ESHUTDOWN:
		return ISC_R_SHUTTINGDOWN;

	      case ETIMEDOUT:
		return ISC_R_TIMEDOUT;

	      case ECONNREFUSED:
		return ISC_R_CONNREFUSED;

	      case EHOSTDOWN:
		return ISC_R_HOSTDOWN;

	      case EHOSTUNREACH:
		return ISC_R_HOSTUNREACH;

#ifdef EDQUOT
	      case EDQUOT:
		return ISC_R_QUOTA;
#endif

#ifdef EBADRPC
	      case EBADRPC:
		return ISC_R_NOTIMPLEMENTED;
#endif

#ifdef ERPCMISMATCH
	      case ERPCMISMATCH:
		return DHCP_R_VERSIONMISMATCH;
#endif

#ifdef EPROGMISMATCH
	      case EPROGMISMATCH:
		return DHCP_R_VERSIONMISMATCH;
#endif

#ifdef EAUTH
	      case EAUTH:
		return DHCP_R_NOTAUTH;
#endif

#ifdef ENEEDAUTH
	      case ENEEDAUTH:
		return DHCP_R_NOTAUTH;
#endif

#ifdef EOVERFLOW
	      case EOVERFLOW:
		return ISC_R_NOSPACE;
#endif
	}
	return ISC_R_UNEXPECTED;
}
