/*
server/ip/gen/icmp.h
*/

#ifndef __SERVER__IP__GEN__ICMP_H__
#define __SERVER__IP__GEN__ICMP_H__

#define ICMP_MIN_HDR_SIZE	4

#define ICMP_TYPE_ECHO_REPL	0
#define ICMP_TYPE_DST_UNRCH	3
#	define ICMP_NET_UNRCH			0
#	define ICMP_HOST_UNRCH			1
#	define ICMP_PROTOCOL_UNRCH		2
#	define ICMP_PORT_UNRCH			3
#	define ICMP_FRAGM_AND_DF		4
#	define ICMP_SOURCE_ROUTE_FAILED		5
#define ICMP_TYPE_SRC_QUENCH	4
#define ICMP_TYPE_REDIRECT	5
#	define ICMP_REDIRECT_NET		0
#	define ICMP_REDIRECT_HOST		1
#	define ICMP_REDIRECT_TOS_AND_NET	2
#	define ICMP_REDIRECT_TOS_AND_HOST	3
#define ICMP_TYPE_ECHO_REQ	8
#define ICMP_TYPE_ROUTER_ADVER	9
#define ICMP_TYPE_ROUTE_SOL	10
#define ICMP_TYPE_TIME_EXCEEDED	11
#	define ICMP_TTL_EXC			0
#	define ICMP_FRAG_REASSEM		1
#define ICMP_TYPE_PARAM_PROBLEM	12
#define ICMP_TYPE_TS_REQ	13
#define ICMP_TYPE_TS_REPL	14
#define ICMP_TYPE_INFO_REQ	15
#define ICMP_TYPE_INFO_REPL	16

/* Preferences for router advertisements. A router daemon installs itself
 * as the default router in the router's interfaces by sending router
 * advertisements to localhost with preference ICMP_RA_LOCAL_PREF.
 */
#define ICMP_RA_DEFAULT_PREF	0x00000000
#define ICMP_RA_INVAL_PREF	0x80000000
#define ICMP_RA_MAX_PREF	0x7fffffff
#define ICMP_RA_LOCAL_PREF	0x10000000

#endif /* __SERVER__IP__GEN__ICMP_H__ */

/*
 * $PchId: icmp.h,v 1.6 2002/06/10 07:10:26 philip Exp $
 */
