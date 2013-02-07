/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998-2003 by Internet Software Consortium
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
 *   http://www.isc.org/
 */

#ifndef ISC_RESULT_H
#define ISC_RESULT_H 1

#include <isc-dhcp/boolean.h>
#include <isc-dhcp/lang.h>
#include <isc-dhcp/list.h>
#include <isc-dhcp/types.h>

ISC_LANG_BEGINDECLS

typedef enum {
	ISC_R_SUCCESS = 0,
	ISC_R_NOMEMORY = 1,
	ISC_R_TIMEDOUT = 2,
	ISC_R_NOTHREADS = 3,
	ISC_R_ADDRNOTAVAIL = 4,
	ISC_R_ADDRINUSE = 5,
	ISC_R_NOPERM = 6,
	ISC_R_NOCONN = 7,
	ISC_R_NETUNREACH = 8,
	ISC_R_HOSTUNREACH = 9,
	ISC_R_NETDOWN = 10,
	ISC_R_HOSTDOWN = 11,
	ISC_R_CONNREFUSED = 12,
	ISC_R_NORESOURCES = 13,
	ISC_R_EOF = 14,
	ISC_R_BOUND = 15,
	ISC_R_TASKDONE = 16,
	ISC_R_LOCKBUSY = 17,
	ISC_R_EXISTS = 18,
	ISC_R_NOSPACE = 19,
	ISC_R_CANCELED = 20,
	ISC_R_TASKNOSEND = 21,
	ISC_R_SHUTTINGDOWN = 22,
	ISC_R_NOTFOUND = 23,
	ISC_R_UNEXPECTEDEND = 24,
	ISC_R_FAILURE = 25,
	ISC_R_IOERROR = 26,
	ISC_R_NOTIMPLEMENTED = 27,
	ISC_R_UNBALANCED = 28,
	ISC_R_NOMORE = 29,
	ISC_R_INVALIDFILE = 30,
	ISC_R_BADBASE64 = 31,
	ISC_R_UNEXPECTEDTOKEN = 32,
	ISC_R_QUOTA = 33,
	ISC_R_UNEXPECTED = 34,
	ISC_R_ALREADYRUNNING = 35,
	ISC_R_HOSTUNKNOWN = 36,
	ISC_R_VERSIONMISMATCH = 37,
	ISC_R_PROTOCOLERROR = 38,
	ISC_R_INVALIDARG = 39,
	ISC_R_NOTCONNECTED = 40,
	ISC_R_NOTYET = 41,
	ISC_R_UNCHANGED = 42,
	ISC_R_MULTIPLE = 43,
	ISC_R_KEYCONFLICT = 44,
	ISC_R_BADPARSE = 45,
	ISC_R_NOKEYS = 46,
	ISC_R_KEY_UNKNOWN = 47,
	ISC_R_INVALIDKEY = 48,
	ISC_R_INCOMPLETE = 49,
	ISC_R_FORMERR = 50,
	ISC_R_SERVFAIL = 51,
	ISC_R_NXDOMAIN = 52,
	ISC_R_NOTIMPL = 53,
	ISC_R_REFUSED = 54,
	ISC_R_YXDOMAIN = 55,
	ISC_R_YXRRSET = 56,
	ISC_R_NXRRSET = 57,
	ISC_R_NOTAUTH = 58,
	ISC_R_NOTZONE = 59,
	ISC_R_BADSIG = 60,
	ISC_R_BADKEY = 61,
	ISC_R_BADTIME = 62,
	ISC_R_NOROOTZONE = 63,
	ISC_R_DESTADDRREQ = 64,
	ISC_R_CROSSZONE = 65,
	ISC_R_NO_TSIG = 66,
	ISC_R_NOT_EQUAL = 67,
	ISC_R_CONNRESET = 68,
	ISC_R_UNKNOWNATTRIBUTE = 69
} isc_result_t;


#define ISC_R_NRESULTS 			70	/* Number of results */

const char *		isc_result_totext(isc_result_t);
isc_result_t		isc_result_register(unsigned int base,
					    unsigned int nresults,
					    char **text,
					    isc_msgcat_t *msgcat,
					    int set);

ISC_LANG_ENDDECLS

#endif /* ISC_RESULT_H */
