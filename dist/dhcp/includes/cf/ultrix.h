/* ultrix.h

   System dependencies for Ultrix 4.2 (tested on 4.2a+multicast)... */

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

/* Ultrix uses the old 4.2BSD-style syslog(). */
#include <sys/syslog.h>
#define SYSLOG_4_2

#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>

extern int h_errno;

#include <net/if.h>

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "/etc/dhcrelay.pid"
#endif

#define int8_t		char
#define int16_t		short 
#define int32_t		long 
#define	ssize_t		long

#define u_int8_t	unsigned char		/* Not quite POSIX... */
#define u_int16_t	unsigned short 
#define u_int32_t	unsigned long 

#define	ssize_t		size_t

/* The jmp_buf type is an array on ultrix, so we can't dereference it
   and must declare it differently. */
#define jbp_decl(x)	jmp_buf x
#define jref(x)		(x)
#define jdref(x)	(x)
#define jrefproto	jmp_buf

#define IPTOS_LOWDELAY		0x10
/*      IPTOS_LOWCOST		0x02 XXX */

/* Varargs stuff... */
#include <varargs.h>
#define VA_DOTDOTDOT va_alist
#define VA_start(list, last) va_start (list)

/* XXX: System is not thought to support snprintf/vsnprintf.  Please verify. */
#define NO_SNPRINTF

#define NEED_INET_ATON

#define INADDR_LOOPBACK	((u_int32_t)0x7f000001)
#define EOL	'\n'
#define VOIDPTR	void *
#define SOCKLEN_T int

/*
 * Time stuff...
 *
 * Definitions for an ISC DHCPD system that uses time_t
 * to represent time internally as opposed to, for example,  struct timeval.)
 */

#define TIME time_t
#define GET_TIME(x)	time ((x))

/* Ultrix doesn't provide an endian.h, but it only runs on little-endian
   machines, so we'll just hack around the issue. */
#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 2
#define BYTE_ORDER LITTLE_ENDIAN

#if defined (USE_DEFAULT_NETWORK)
#  define USE_UPF
#endif

#ifdef NEED_PRAND_CONF
const char *cmds[] = {
	"/bin/ps -aux 2>&1",
	"/usr/etc/arp -an 2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/usr/bin/df  2>&1",
	"/usr/ucb/uptime  2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/usr/bin/iostat  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/var/tmp",
	".",
	"/",
	"/var/spool",
	"/var/adm",
	"/dev",
	"/var/spool/mail",
	NULL
};

const char *files[] = {
	"/var/spool/mqueue/syslog",
	"/var/adm/wtmp",
	"/var/adm/lastlog",
	NULL
};
#endif /* NEED_PRAND_CONF */
