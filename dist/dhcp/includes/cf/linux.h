/* linux.h

   System dependencies for Linux.

   Based on a configuration originally supplied by Jonathan Stone. */

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

#include <features.h>
#ifndef __BIT_TYPES_DEFINED__
#define __BIT_TYPES_DEFINED__
#undef __USE_BSD
typedef char int8_t;
typedef short int16_t;
typedef long int32_t;

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned long u_int32_t;
#endif /* __BIT_TYPES_DEFINED__ */

typedef u_int8_t u8;
typedef u_int16_t u16;
typedef u_int32_t u32;

#include <syslog.h>
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
#include <net/route.h>

#if LINUX_MAJOR == 1
# include <linux/if_arp.h>
# include <linux/time.h>		/* also necessary */
#else
# include <net/if_arp.h>
#endif

#include <sys/time.h>		/* gettimeofday()*/

/* Databases go in /var/state/dhcp.   It would also be valid to put them
   in /var/state/misc - indeed, given that there's only one lease file, it
   would probably be better.   However, I have some ideas for optimizing
   the lease database that may result in a _lot_ of smaller files being
   created, so in that context it makes more sense to have a seperate
   directory. */

#ifndef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB		"/var/state/dhcp/dhcpd.leases"
#endif

#ifndef _PATH_DHCLIENT_DB
#define _PATH_DHCLIENT_DB	"/var/state/dhcp/dhclient.leases"
#endif

/* Varargs stuff... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define VA_start(list, last) va_start (list, last)
#define va_dcl

#define VOIDPTR	void *

#define EOL	'\n'

/* Time stuff... */

#include <time.h>

#define TIME time_t
#define GET_TIME(x)	time ((x))

#if (LINUX_MAJOR >= 2)
# if (LINUX_MINOR >= 1)
#  if defined (USE_DEFAULT_NETWORK)
#   define USE_LPF
#  endif
#  if !defined (__sparc__)	/* XXX hopefully this will be fixed someday */
#   define SIOCGIFCONF_ZERO_PROBE
#  endif
#  define LINUX_SLASHPROC_DISCOVERY
#  define PROCDEV_DEVICE "/proc/net/dev"
#  define HAVE_ARPHRD_TUNNEL
#  define HAVE_TR_SUPPORT
# endif
# define HAVE_ARPHRD_METRICOM
# define HAVE_ARPHRD_IEEE802
# define HAVE_ARPHRD_LOOPBACK
# define HAVE_SO_BINDTODEVICE
# define HAVE_SIOCGIFHWADDR
# define HAVE_SETFD
#endif

#if defined (SIOCGIFHWADDR) && !defined (HAVE_SIOCGIFHWADDR)
# define HAVE_SIOCGIFHWADDR
#endif

#if !defined (USE_LPF)
# if defined (USE_DEFAULT_NETWORK)
#  define USE_SOCKETS
#  define SOCKET_CAN_RECEIVE_UNICAST_UNCONFIGURED
# endif
# define IGNORE_HOSTUNREACH
#endif

#define ALIAS_NAMES_PERMUTED
#define SKIP_DUMMY_INTERFACES

#ifdef NEED_PRAND_CONF
#ifndef HAVE_DEV_RANDOM
 # define HAVE_DEV_RANDOM 1
 #endif /* HAVE_DEV_RANDOM */

const char *cmds[] = {
	"/bin/ps -axlw 2>&1",
	"/sbin/arp -an 2>&1",
	"/bin/netstat -an 2>&1",
	"/bin/df  2>&1",
	"/usr/bin/dig com. soa +ti=1 +retry=0 2>&1",
	"/usr/bin/uptime  2>&1",
	"/bin/netstat -s 2>&1",
	"/usr/bin/dig . soa +ti=1 +retry=0 2>&1",
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
	"/dev",
	"/var/spool/mail",
	"/home",
	"/usr/home",
	NULL
};

const char *files[] = {
	"/proc/stat",
	"/proc/rtc",
	"/proc/meminfo",
	"/proc/interrupts",
	"/proc/self/status",
	"/var/log/messages",
	"/var/log/wtmp",
	"/var/log/lastlog",
	NULL
};
#endif /* NEED_PRAND_CONF */
