/*
net/gen/rip.h

Definitions for the Routing Information Protocol (RFC-1058).

Created:	Aug 16, 1993 by Philip Homburg <philip@cs.vu.nl>
*/

#ifndef NET__GEN__RIP_H
#define NET__GEN__RIP_H

typedef struct rip_hdr
{
	u8_t rh_command;
	u8_t rh_version;
	u16_t rh_zero;
} rip_hdr_t;

#define RHC_REQUEST	1
#define RHC_RESPONSE	2

#define RIP_ENTRY_MAX	25

typedef struct rip_entry
{
	union
	{
		struct rip_entry_v1
		{
			u16_t re_family;
			u16_t re_zero0;
			u32_t re_address;
			u32_t re_zero1;
			u32_t re_zero2;
			u32_t re_metric;
		} v1;
		struct rip_entry_v2
		{
			u16_t re_family;
			u16_t re_tag;
			u32_t re_address;
			u32_t re_mask;
			u32_t re_nexthop;
			u32_t re_metric;
		} v2;
	} u;
} rip_entry_t;

#define RIP_FAMILY_IP	2
#define RIP_INFINITY	16

#define RIP_UDP_PORT	520
#define RIP_PERIOD	 30	/* A responce is sent once every
				 * RIP_PERIOD seconds
				 */
#define RIP_FUZZ	 10	/* The actual value used is RIP_FREQUENCE -
				 * a random number of at most RIP_FUZZ.
				 */
#define RIP_TIMEOUT	180	/* A route is dead after RIP_TIMEOUT seconds */
#define RIP_DELETE_TO	120	/* A dead route is removed after RIP_DELETE_TO
				 * seconds
				 */

#ifdef __RIP_DEBUG
#undef RIP_PERIOD
#define RIP_PERIOD	15
#undef RIP_TIMEOUT
#define RIP_TIMEOUT	10
#undef RIP_DELETE_TO
#define RIP_DELETE_TO	10
#endif /* __RIP_DEBUG */

#endif /* NET__GEN__RIP_H */

/*
 * $PchId: rip.h,v 1.3 1995/11/17 22:21:16 philip Exp $
 */
