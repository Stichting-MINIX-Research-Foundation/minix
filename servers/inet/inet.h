/*
inet/inet.h

Created:	Dec 30, 1991 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef INET__INET_H
#define INET__INET_H

#define _SYSTEM	1	/* get OK and negative error codes */

#include <sys/types.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/ioc_file.h>
#include <sys/time.h>
#include <minix/config.h>
#include <minix/type.h>

#define _NORETURN	/* Should be non empty for GCC */

typedef int ioreq_t;

#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_hdr.h>
#include <net/gen/eth_io.h>
#include <net/gen/in.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/ip_io.h>
#include <net/gen/icmp.h>
#include <net/gen/icmp_hdr.h>
#include <net/gen/oneCsum.h>
#include <net/gen/psip_hdr.h>
#include <net/gen/psip_io.h>
#include <net/gen/route.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>

#include <net/gen/arp_io.h>
#ifdef __NBSD_LIBC
#include <sys/ioc_net.h>
#else
#include <net/ioctl.h>
#endif

#include "const.h"
#include "inet_config.h"

#define PUBLIC
#define EXTERN	extern
#define PRIVATE	static
#define FORWARD	static

#define THIS_FILE static char *this_file= __FILE__;

void panic0(char *file, int line);
void inet_panic(void) _NORETURN;

#define ip_panic(print_list)  \
	(panic0(this_file, __LINE__), printf print_list, panic())
#define panic() inet_panic()

#if DEBUG
#define ip_warning(print_list)  \
	( \
		printf("warning at %s, %d: ", this_file, __LINE__), \
		printf print_list, \
		printf("\ninet stacktrace: "), \
		util_stacktrace() \
	)
#else
#define ip_warning(print_list)	((void) 0)
#endif

#define DBLOCK(level, code) \
	do { if ((level) & DEBUG) { where(); code; } } while(0)
#define DIFBLOCK(level, condition, code) \
	do { if (((level) & DEBUG) && (condition)) \
		{ where(); code; } } while(0)

extern int this_proc;
extern char version[];

#ifndef HZ
EXTERN u32_t system_hz;
#define HZ system_hz
#define HZ_DYNAMIC 1
#endif

#endif /* INET__INET_H */

/*
 * $PchId: inet.h,v 1.16 2005/06/28 14:27:54 philip Exp $
 */
