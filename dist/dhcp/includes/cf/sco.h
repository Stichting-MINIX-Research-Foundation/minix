/* sco.h

   System dependencies for SCO ODT 3.0...

   Based on changes contributed by Gerald Rosenberg. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
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
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <syslog.h>
#include <sys/types.h>

/* Basic Integer Types not defined in SCO headers... */

typedef char int8_t;
typedef short int16_t;
typedef long int32_t; 

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t; 
typedef unsigned long u_int32_t;

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>

extern int h_errno;

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* XXX dunno if this is required for SCO... */
/*
 * Definitions for IP type of service (ip_tos)
 */
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
/*      IPTOS_LOWCOST           0x02 XXX */

/* SCO doesn't have /var/run. */
#ifndef _PATH_DHCPD_CONF
#define _PATH_DHCPD_CONF	"/etc/dhcpd.conf"
#endif
#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID 	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID  "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID  "/etc/dhcrelay.pid"
#endif
#ifndef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB      "/etc/dhcpd.leases"
#endif
#ifndef _PATH_DHCLIENT_DB
#define _PATH_DHCLIENT_DB   "/etc/dhclient.leases"
#endif


#if !defined (INADDR_LOOPBACK)
#define INADDR_LOOPBACK	((u_int32_t)0x7f000001)
#endif

/* Varargs stuff: use stdarg.h instead ... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define VA_start(list, last) va_start (list, last)
#define va_dcl

/* SCO doesn't support limited sprintfs. */
#define NO_SNPRINTF

/* By default, use BSD Socket API for receiving and sending packets.
   This actually works pretty well on Solaris, which doesn't censor
   the all-ones broadcast address. */
#if defined (USE_DEFAULT_NETWORK)
# define USE_SOCKETS
#endif

#define EOL	'\n'
#define VOIDPTR	void *

/* socklen_t */
typedef int socklen_t;

/*
 * Time stuff...
 *
 * Definitions for an ISC DHCPD system that uses time_t
 * to represent time internally as opposed to, for example,  struct timeval.)
 */

#include <time.h>
#include <sys/time.h>

#define TIME time_t
#define GET_TIME(x)	time ((x))

#ifdef NEED_PRAND_CONF
const char *cmds[] = {
	"/bin/ps -ef 2>&1",
	"/etc/arp -n -a 2>&1",
	"/usr/bin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/uptime  2>&1",
	"/usr/bin/netstat -s 2>&1",
	"/usr/bin/vmstat  2>&1",
	"/usr/bin/w  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/var/spool",
	"/var/adm",
	"/dev",
	NULL
};

const char *files[] = {
	NULL
};
#endif /* NEED_PRAND_CONF */
