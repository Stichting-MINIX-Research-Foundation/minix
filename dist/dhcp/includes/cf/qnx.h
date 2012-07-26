/* qnx.h

   System dependencies for QNX...  */

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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <limits.h>
#include <syslog.h>
#include <sys/select.h>

#include <sys/wait.h>
#include <signal.h>

#ifdef __QNXNTO__
#include <sys/param.h>
#endif

#include <netdb.h>
extern int h_errno;

#include <net/if.h>
#ifndef __QNXNTO__
# define INADDR_LOOPBACK ((u_long)0x7f000001)
#endif

/* Varargs stuff... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define va_dcl
#define VA_start(list, last) va_start (list, last)

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "/etc/dhcrelay.pid"
#endif

#define EOL	'\n'
#define VOIDPTR void *

/* Time stuff... */
#include <sys/time.h>
#define TIME time_t
#define GET_TIME(x)	time ((x))
#define TIME_DIFF(high, low)	 	(*(high) - *(low))
#define SET_TIME(x, y)	(*(x) = (y))
#define ADD_TIME(d, s1, s2) (*(d) = *(s1) + *(s2))
#define SET_MAX_TIME(x)	(*(x) = INT_MAX)

#ifndef __QNXNTO__
typedef unsigned char	u_int8_t;
typedef unsigned short	u_int16_t;
typedef unsigned long	u_int32_t;
typedef signed short	int16_t;
typedef signed long	int32_t;
#endif

#ifdef __QNXNTO__
typedef int socklen_t;
#endif

#define strcasecmp( s1, s2 )			stricmp( s1, s2 )
#define strncasecmp( s1, s2, n )		strnicmp( s1, s2, n )
#define random()				rand()

#define HAVE_SA_LEN
#define BROKEN_TM_GMT
#define USE_SOCKETS
#undef AF_LINK

#ifndef __QNXNTO__
# define NO_SNPRINTF
#endif

#ifdef __QNXNTO__
# define GET_HOST_ID_MISSING
#endif

/*
    NOTE: to get the routing of the 255.255.255.255 broadcasts to work
    under QNX, you need to issue the following command before starting
    the daemon:

    	route add -interface 255.255.255.0 <hostname>

    where <hostname> is replaced by the hostname or IP number of the
    machine that dhcpd is running on.
*/

#ifndef __QNXNTO__
# if defined (NSUPDATE)
# error NSUPDATE is not supported on QNX at this time!!
# endif
#endif


#ifdef NEED_PRAND_CONF
#ifndef HAVE_DEV_RANDOM
/* You should find and install the /dev/random driver */
 # define HAVE_DEV_RANDOM 1
 #endif /* HAVE_DEV_RANDOM */

const char *cmds[] = {
        "/bin/ps -a 2>&1",
	"/bin/sin 2>&1",
        "/sbin/arp -an 2>&1",
        "/bin/netstat -an 2>&1",
        "/bin/df  2>&1",
	"/bin/sin fds 2>&1",
        "/bin/netstat -s 2>&1",
	"/bin/sin memory 2>&1",
        NULL
};

const char *dirs[] = {
        "/tmp",
        ".",
        "/",
        "/var/spool",
        "/dev",
        "/var/spool/mail",
        "/home",
        NULL
};

const char *files[] = {
        "/proc/ipstats",
        "/proc/dumper",
        "/proc/self/as",
        "/var/log/messages",
        NULL
};
#endif /* NEED_PRAND_CONF */

