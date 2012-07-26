/* isclib.h

   connections to the isc and dns libraries */

/*
 * Copyright (c) 2009 by Internet Systems Consortium, Inc. ("ISC")
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
 *
 */

#ifndef ISCLIB_H
#define ISCLIB_H

#include "config.h"

#include <syslog.h>

#define MAXWIRE 256

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <isc/buffer.h>
#include <isc/lex.h>
#include <isc/lib.h>
#include <isc/app.h>
#include <isc/mem.h>
#include <isc/parseint.h>
#include <isc/socket.h>
#include <isc/sockaddr.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/heap.h>
#include <isc/random.h>

#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/lib.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/tsec.h>

#include <dst/dst.h>

#include "result.h"


/*
 * DHCP context structure
 * This holds the libisc information for a dhcp entity
 */

typedef struct dhcp_context {
	isc_mem_t	*mctx;
	isc_appctx_t	*actx;
	int              actx_started;
	isc_taskmgr_t	*taskmgr;
	isc_task_t	*task;
	isc_socketmgr_t *socketmgr;
	isc_timermgr_t	*timermgr;
#if defined (NSUPDATE)
  	dns_client_t    *dnsclient;
#endif
} dhcp_context_t;

extern dhcp_context_t dhcp_gbl_ctx;

#define DHCP_MAXDNS_WIRE 256
#define DHCP_MAXNS         3
#define DHCP_HMAC_MD5_NAME "HMAC-MD5.SIG-ALG.REG.INT."

isc_result_t dhcp_isc_name(unsigned char    *namestr,
			   dns_fixedname_t  *namefix,
			   dns_name_t      **name);

isc_result_t
isclib_make_dst_key(char          *inname,
		    char          *algorithm,
		    unsigned char *secret,
		    int            length,
		    dst_key_t    **dstkey);

isc_result_t dhcp_context_create(void);
void isclib_cleanup(void);

#endif /* ISCLIB_H */
