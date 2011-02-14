/*
server/ip/gen/route.h
*/

#ifndef __SERVER__IP__GEN__ROUTE_H__
#define __SERVER__IP__GEN__ROUTE_H__

typedef struct nwio_route
{
	u32_t nwr_ent_no;
	u32_t nwr_ent_count;
	ipaddr_t nwr_dest;
	ipaddr_t nwr_netmask;
	ipaddr_t nwr_gateway;
	u32_t nwr_dist;
	u32_t nwr_flags;
	u32_t nwr_pref;
	u32_t nwr_mtu;		/* Ignored, compatibility with VMD */
	ipaddr_t nwr_ifaddr;
} nwio_route_t;

#define NWRF_EMPTY		0
#define NWRF_INUSE		1
#define NWRF_STATIC		2
#define NWRF_UNREACHABLE	4

#endif /* __SERVER__IP__GEN__ROUTE_H__ */

/*
 * $PchId: route.h,v 1.3 1995/11/17 22:19:50 philip Exp $
 */
