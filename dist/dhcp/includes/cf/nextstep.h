/* nextstep.h

   System dependencies for NEXTSTEP 3 & 4 (tested on 4.2PR2)... */

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

/* NeXT needs BSD44 ssize_t */
typedef int		ssize_t;
/* NeXT doesn't have BSD setsid() */
#define setsid getpid
#import <sys/types.h>
/* Porting::
   The jmp_buf type as declared in <setjmp.h> is sometimes a structure
   and sometimes an array.   By default, we assume it's a structure.
   If it's an array on your system, you may get compile warnings or errors
   as a result in confpars.c.   If so, try including the following definitions,
   which treat jmp_buf as an array: */
#if 0
#define jbp_decl(x)	jmp_buf x
#define jref(x)		(x)
#define jdref(x)	(x)
#define jrefproto	jmp_buf
#endif
#import <syslog.h>
#import <string.h>
#import <errno.h>
#import <unistd.h>
#import <sys/wait.h>
#import <signal.h>
#import <setjmp.h>
#import <limits.h>
extern int h_errno;
#import <net/if.h>
#import <net/if_arp.h>
/* Porting::
   Some older systems do not have defines for IP type-of-service,
   or don't define them the way we expect.   If you get undefined
   symbol errors on the following symbols, they probably need to be
   defined here. */
#if 0
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
#endif

#if !defined (_PATH_DHCPD_PID)
# define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif

#if !defined (_PATH_DHCLIENT_PID)
# define _PATH_DHCLIENT_PID	"/etc/dhclient.pid"
#endif

#if !defined (_PATH_DHCRELAY_PID)
# define _PATH_DHCRELAY_PID	"/etc/dhcrelay.pid"
#endif

/* Stdarg definitions for ANSI-compliant C compilers. */
#import <stdarg.h>
#define VA_DOTDOTDOT ...
#define VA_start(list, last) va_start (list, last)
#define va_dcl

/* NeXT lacks snprintf */
#define NO_SNPRINTF

/* Porting::
   You must define the default network API for your port.   This
   will depend on whether one of the existing APIs will work for
   you, or whether you need to implement support for a new API.
   Currently, the following APIs are supported:
   	The BSD socket API: define USE_SOCKETS.
	The Berkeley Packet Filter: define USE_BPF.
	The Streams Network Interface Tap (NIT): define USE_NIT.
	Raw sockets: define USE_RAW_SOCKETS
   If your system supports the BSD socket API and doesn't provide
   one of the supported interfaces to the physical packet layer,
   you can either provide support for the low-level API that your
   system does support (if any) or just use the BSD socket interface.
   The BSD socket interface doesn't support multiple network interfaces,
   and on many systems, it does not support the all-ones broadcast
   address, which can cause problems with some DHCP clients (e.g.
   Microsoft Windows 95). */
#define USE_BPF
#if 0
#if defined (USE_DEFAULT_NETWORK)
#  define USE_SOCKETS
#endif
#endif
#define EOL '\n'
#define VOIDPTR void *
#import <time.h>
#define TIME time_t
#define GET_TIME(x)	time ((x))

#ifdef NEED_PRAND_CONF
const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/usr/etc/arp -a 2>&1",
	"/usr/ucb/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/local/bin/dig com. soa +ti=1 2>&1",
	"/usr/ucb/uptime  2>&1",
	"/usr/ucb/printenv  2>&1",
	"/usr/ucb/netstat -s 2>&1",
	"/usr/local/bin/dig . soa +ti=1 2>&1",
	"/usr/bin/iostat  2>&1",
	"/usr/bin/vm_stat  2>&1",
	"/usr/ucb/w  2>&1",
	NULL
};

const char *dirs[] = {
	"/tmp",
	"/usr/tmp",
	".",
	"/",
	"/usr/spool",
	"/dev",
	NULL
};

const char *files[] = {
	"/usr/adm/messages",
	"/usr/adm/wtmp",
	"/usr/adm/lastlog",
	NULL
};
#endif /* NEED_PRAND_CONF */
