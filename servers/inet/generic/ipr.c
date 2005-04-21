/*
ipr.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "clock.h"

#include "type.h"
#include "assert.h"
#include "buf.h"
#include "event.h"
#include "io.h"
#include "ip_int.h"
#include "ipr.h"

THIS_FILE

#define OROUTE_NR		32
#define OROUTE_STATIC_NR	16
#define OROUTE_HASH_ASS_NR	 4
#define OROUTE_HASH_NR		32
#define OROUTE_HASH_MASK	(OROUTE_HASH_NR-1)

#define hash_oroute(port_nr, ipaddr, hash_tmp) (hash_tmp= (ipaddr), \
	hash_tmp= (hash_tmp >> 20) ^ hash_tmp, \
	hash_tmp= (hash_tmp >> 10) ^ hash_tmp, \
	hash_tmp= (hash_tmp >> 5) ^ hash_tmp, \
	(hash_tmp + (port_nr)) & OROUTE_HASH_MASK)

typedef struct oroute_hash
{
	ipaddr_t orh_addr;
	oroute_t *orh_route;
} oroute_hash_t;

PRIVATE oroute_t oroute_table[OROUTE_NR];
PRIVATE oroute_t *oroute_head;
PRIVATE int static_oroute_nr;
PRIVATE oroute_hash_t oroute_hash_table[OROUTE_HASH_NR][OROUTE_HASH_ASS_NR];

#define IROUTE_NR		(sizeof(int) == 2 ? 64 : 512)
#define IROUTE_HASH_ASS_NR	 4
#define IROUTE_HASH_NR		32
#define IROUTE_HASH_MASK	(IROUTE_HASH_NR-1)

#define hash_iroute(port_nr, ipaddr, hash_tmp) (hash_tmp= (ipaddr), \
	hash_tmp= (hash_tmp >> 20) ^ hash_tmp, \
	hash_tmp= (hash_tmp >> 10) ^ hash_tmp, \
	hash_tmp= (hash_tmp >> 5) ^ hash_tmp, \
	(hash_tmp + (port_nr)) & IROUTE_HASH_MASK)

typedef struct iroute_hash
{
	ipaddr_t irh_addr;
	iroute_t *irh_route;
} iroute_hash_t;

PRIVATE iroute_t iroute_table[IROUTE_NR];
PRIVATE iroute_hash_t iroute_hash_table[IROUTE_HASH_NR][IROUTE_HASH_ASS_NR];

FORWARD oroute_t *oroute_find_ent ARGS(( int port_nr, ipaddr_t dest ));
FORWARD void oroute_del ARGS(( oroute_t *oroute ));
FORWARD oroute_t *sort_dists ARGS(( oroute_t *oroute ));
FORWARD oroute_t *sort_gws ARGS(( oroute_t *oroute ));
FORWARD	oroute_uncache_nw ARGS(( ipaddr_t dest, ipaddr_t netmask ));
FORWARD	iroute_uncache_nw ARGS(( ipaddr_t dest, ipaddr_t netmask ));

PUBLIC void ipr_init()
{
	int i;
	oroute_t *oroute;
	iroute_t *iroute;

#if ZERO
	for (i= 0, oroute= oroute_table; i<OROUTE_NR; i++, oroute++)
		oroute->ort_flags= ORTF_EMPTY;
	static_oroute_nr= 0;
#endif
	assert(OROUTE_HASH_ASS_NR == 4);

#if ZERO
	for (i= 0, iroute= iroute_table; i<IROUTE_NR; i++, iroute++)
		iroute->irt_flags= IRTF_EMPTY;
#endif
	assert(IROUTE_HASH_ASS_NR == 4);
}


PUBLIC iroute_t *iroute_frag(port_nr, dest)
int port_nr;
ipaddr_t dest;
{
	int hash, i, r_hash_ind;
	iroute_hash_t *iroute_hash;
	iroute_hash_t tmp_hash;
	iroute_t *iroute, *bestroute;
	time_t currtim;
	unsigned long hash_tmp;

	currtim= get_time();

	hash= hash_iroute(port_nr, dest, hash_tmp);
	iroute_hash= &iroute_hash_table[hash][0];
	if (iroute_hash[0].irh_addr == dest)
		iroute= iroute_hash[0].irh_route;
	else if (iroute_hash[1].irh_addr == dest)
	{
		tmp_hash= iroute_hash[1];
		iroute_hash[1]= iroute_hash[0];
		iroute_hash[0]= tmp_hash;
		iroute= tmp_hash.irh_route;
	}
	else if (iroute_hash[2].irh_addr == dest)
	{
		tmp_hash= iroute_hash[2];
		iroute_hash[2]= iroute_hash[1];
		iroute_hash[1]= iroute_hash[0];
		iroute_hash[0]= tmp_hash;
		iroute= tmp_hash.irh_route;
	}
	else if (iroute_hash[3].irh_addr == dest)
	{
		tmp_hash= iroute_hash[3];
		iroute_hash[3]= iroute_hash[2];
		iroute_hash[2]= iroute_hash[1];
		iroute_hash[1]= iroute_hash[0];
		iroute_hash[0]= tmp_hash;
		iroute= tmp_hash.irh_route;
	}
	else
		iroute= NULL;
	if (iroute)
		return iroute;

	bestroute= NULL;
	for (i= 0, iroute= iroute_table; i < IROUTE_NR; i++, iroute++)
	{
		if (!(iroute->irt_flags & IRTF_INUSE))
			continue;
		if (((dest ^ iroute->irt_dest) & iroute->irt_subnetmask) != 0)
			continue;
		if (!bestroute)
		{
			bestroute= iroute;
			continue;
		}

		/* More specific netmasks are better */
		if (iroute->irt_subnetmask != bestroute->irt_subnetmask)
		{
			if (ntohl(iroute->irt_subnetmask) > 
				ntohl(bestroute->irt_subnetmask))
			{
				bestroute= iroute;
			}
			continue;
		}
			
		/* Dynamic routes override static routes */
		if ((iroute->irt_flags & IRTF_STATIC) != 
			(bestroute->irt_flags & IRTF_STATIC))
		{
			if (bestroute->irt_flags & IRTF_STATIC)
				bestroute= iroute;
			continue;
		}

		/* A route to the local interface give an opportunity
		 * to send redirects.
		 */
		if (iroute->irt_port != bestroute->irt_port)
		{
			if (iroute->irt_port == port_nr)
				bestroute= iroute;
			continue;
		}
	}
	if (bestroute == NULL)
		return NULL;

	iroute_hash[3]= iroute_hash[2];
	iroute_hash[2]= iroute_hash[1];
	iroute_hash[1]= iroute_hash[0];
	iroute_hash[0].irh_addr= dest;
	iroute_hash[0].irh_route= bestroute;

	return bestroute;
}

PUBLIC int oroute_frag(port_nr, dest, ttl, nexthop)
int port_nr;
ipaddr_t dest;
int ttl;
ipaddr_t *nexthop;
{
	oroute_t *oroute;

	oroute= oroute_find_ent(port_nr, dest);
	if (!oroute || oroute->ort_dist > ttl)
		return EDSTNOTRCH;

	*nexthop= oroute->ort_gateway;
	return NW_OK;
}


PUBLIC int ipr_add_oroute(port_nr, dest, subnetmask, gateway, 
	timeout, dist, static_route, preference, oroute_p)
int port_nr;
ipaddr_t dest;
ipaddr_t subnetmask;
ipaddr_t gateway;
time_t timeout;
int dist;
int static_route;
i32_t preference;
oroute_t **oroute_p;
{
	int i;
	ip_port_t *ip_port;
	oroute_t *oroute, *oldest_route, *prev, *nw_route, *gw_route, 
		*prev_route;
	time_t currtim;

	oldest_route= 0;
	currtim= get_time();

	DBLOCK(0x10, 
		printf("adding oroute to "); writeIpAddr(dest);
		printf("["); writeIpAddr(subnetmask); printf("] through ");
		writeIpAddr(gateway);
		printf(" timeout: %lds, distance %d\n",
			(long)timeout/HZ, dist));

	ip_port= &ip_port_table[port_nr];

	/* Validate gateway */
	if (((gateway ^ ip_port->ip_ipaddr) & ip_port->ip_subnetmask) != 0)
	{
		DBLOCK(2, printf("ipr_add_oroute: invalid gateway: "); writeIpAddr(gateway); printf("\n"));
		return EINVAL;
	}

	if (static_route)
	{
		if (static_oroute_nr >= OROUTE_STATIC_NR)
			return ENOMEM;
		static_oroute_nr++;
	}
	else
	{
		/* Try to track down any old routes. */
		for(oroute= oroute_head; oroute; oroute= oroute->ort_nextnw)
		{
			if (oroute->ort_port != port_nr)
				continue;
			if (oroute->ort_dest == dest &&
				oroute->ort_subnetmask == subnetmask)
			{
				break;
			}
		}
		for(; oroute; oroute= oroute->ort_nextgw)
		{
			if (oroute->ort_gateway == gateway)
				break;
		}
		for(; oroute; oroute= oroute->ort_nextdist)
		{
			if ((oroute->ort_flags & ORTF_STATIC) != 0)
				continue;
			if (oroute->ort_dist > dist)
				continue;
			if (oroute->ort_dist == dist && 
				oroute->ort_pref == preference)
			{
				if (timeout)
					oroute->ort_exp_tim= currtim + timeout;
				else
					oroute->ort_exp_tim= 0;
				oroute->ort_timestamp= currtim;
				assert(oroute->ort_port == port_nr);
				if (oroute_p != NULL)
					*oroute_p= oroute;
				return NW_OK;
			}
			break;
		}
		if (oroute)
		{
			assert(oroute->ort_port == port_nr);
			oroute_del(oroute);
			oroute->ort_flags= 0;
			oldest_route= oroute;
		}
	}

	if (oldest_route == NULL)
	{
		/* Look for an unused entry, or remove an existing one */
		for (i= 0, oroute= oroute_table; i<OROUTE_NR; i++, oroute++)
		{
			if ((oroute->ort_flags & ORTF_INUSE) == 0)
				break;
			if (oroute->ort_exp_tim && oroute->ort_exp_tim < 
				currtim)
			{
				oroute_del(oroute);
				oroute->ort_flags= 0;
				break;
			}
			if (oroute->ort_flags & ORTF_STATIC)
				continue;
			if (oroute->ort_dest == 0)
			{
				/* Never remove default routes. */
				continue;
			}
			if (oldest_route == NULL)
			{
				oldest_route= oroute;
				continue;
			}
			if (oroute->ort_timestamp < oldest_route->ort_timestamp)
			{
				oldest_route= oroute;
			}
		}
		if (i < OROUTE_NR)
			oldest_route= oroute;
		else
		{
			assert(oldest_route);
			oroute_del(oldest_route);
			oldest_route->ort_flags= 0;
		}
	}

	oldest_route->ort_dest= dest;
	oldest_route->ort_gateway= gateway;
	oldest_route->ort_subnetmask= subnetmask;
	if (timeout)
		oldest_route->ort_exp_tim= currtim + timeout;
	else
		oldest_route->ort_exp_tim= 0;
	oldest_route->ort_timestamp= currtim;
	oldest_route->ort_dist= dist;
	oldest_route->ort_port= port_nr;
	oldest_route->ort_flags= ORTF_INUSE;
	oldest_route->ort_pref= preference;
	if (static_route)
		oldest_route->ort_flags |= ORTF_STATIC;
	
	/* Insert the route by tearing apart the routing table, 
	 * and insert the entry during the reconstruction.
	 */
	for (prev= 0, nw_route= oroute_head; nw_route; 
				prev= nw_route, nw_route= nw_route->ort_nextnw)
	{
		if (nw_route->ort_port != port_nr)
			continue;
		if (nw_route->ort_dest == dest &&
					nw_route->ort_subnetmask == subnetmask)
		{
			if (prev)
				prev->ort_nextnw= nw_route->ort_nextnw;
			else
				oroute_head= nw_route->ort_nextnw;
			break;
		}
	}
	prev_route= nw_route;
	for(prev= NULL, gw_route= nw_route; gw_route; 
				prev= gw_route, gw_route= gw_route->ort_nextgw)
	{
		if (gw_route->ort_gateway == gateway)
		{
			if (prev)
				prev->ort_nextgw= gw_route->ort_nextgw;
			else
				nw_route= gw_route->ort_nextgw;
			break;
		}
	}
	oldest_route->ort_nextdist= gw_route;
	gw_route= oldest_route;
	gw_route= sort_dists(gw_route);
	gw_route->ort_nextgw= nw_route;
	nw_route= gw_route;
	nw_route= sort_gws(nw_route);
	nw_route->ort_nextnw= oroute_head;
	oroute_head= nw_route;
	if (nw_route != prev_route)
		oroute_uncache_nw(nw_route->ort_dest, nw_route->ort_subnetmask);
	if (oroute_p != NULL)
		*oroute_p= oldest_route;
	return NW_OK;
}


PUBLIC void ipr_gateway_down(port_nr, gateway, timeout)
int port_nr;
ipaddr_t gateway;
time_t timeout;
{
	oroute_t *route_ind;
	time_t currtim;
	int i;
	int result;

	currtim= get_time();
	for (i= 0, route_ind= oroute_table; i<OROUTE_NR; i++, route_ind++)
	{
		if (!(route_ind->ort_flags & ORTF_INUSE))
			continue;
		if (route_ind->ort_gateway != gateway)
			continue;
		if (route_ind->ort_exp_tim && route_ind->ort_exp_tim < currtim)
			continue;
		result= ipr_add_oroute(port_nr, route_ind->ort_dest, 
			route_ind->ort_subnetmask, gateway, 
			timeout, ORTD_UNREACHABLE, FALSE, 0, NULL);
		assert(result == NW_OK);
	}
}


PUBLIC void ipr_destunrch(port_nr, dest, netmask, timeout)
int port_nr;
ipaddr_t dest;
ipaddr_t netmask;
time_t timeout;
{
	oroute_t *oroute;
	int result;

	oroute= oroute_find_ent(port_nr, dest);

	if (!oroute)
	{
		DBLOCK(1, printf("got a dest unreachable for ");
			writeIpAddr(dest); printf("but no route present\n"));

		return;
	}
	result= ipr_add_oroute(port_nr, dest, netmask, oroute->ort_gateway, 
		timeout, ORTD_UNREACHABLE, FALSE, 0, NULL);
	assert(result == NW_OK);
}


PUBLIC void ipr_redirect(port_nr, dest, netmask, old_gateway, new_gateway, 
	timeout)
int port_nr;
ipaddr_t dest;
ipaddr_t netmask;
ipaddr_t old_gateway;
ipaddr_t new_gateway;
time_t timeout;
{
	oroute_t *oroute;
	int result;

	oroute= oroute_find_ent(port_nr, dest);

	if (!oroute)
	{
		DBLOCK(1, printf("got a redirect for ");
			writeIpAddr(dest); printf("but no route present\n"));
		return;
	}
	if (oroute->ort_gateway != old_gateway)
	{
		DBLOCK(1, printf("got a redirect from ");
			writeIpAddr(old_gateway); printf(" for ");
			writeIpAddr(dest); printf(" but curr gateway is ");
			writeIpAddr(oroute->ort_gateway); printf("\n"));
		return;
	}
	if (oroute->ort_flags & ORTF_STATIC)
	{
		if (oroute->ort_dest == dest)
		{
			DBLOCK(1, printf("got a redirect for ");
				writeIpAddr(dest);
				printf("but route is fixed\n"));
			return;
		}
	}
	else
	{
		result= ipr_add_oroute(port_nr, dest, netmask, 
			oroute->ort_gateway, HZ, ORTD_UNREACHABLE, 
			FALSE, 0, NULL);
		assert(result == NW_OK);
	}
	result= ipr_add_oroute(port_nr, dest, netmask, new_gateway,
		timeout, 1, FALSE, 0, NULL);
	assert(result == NW_OK);
}


PUBLIC void ipr_ttl_exc(port_nr, dest, netmask, timeout)
int port_nr;
ipaddr_t dest;
ipaddr_t netmask;
time_t timeout;
{
	oroute_t *oroute;
	int new_dist;
	int result;

	oroute= oroute_find_ent(port_nr, dest);

	if (!oroute)
	{
		DBLOCK(1, printf("got a ttl exceeded for ");
			writeIpAddr(dest); printf("but no route present\n"));
		return;
	}

	new_dist= oroute->ort_dist * 2;
	if (new_dist>IP_MAX_TTL)
	{
		new_dist= oroute->ort_dist+1;
		if (new_dist>IP_MAX_TTL)
		{
			DBLOCK(1, printf("got a ttl exceeded for ");
				writeIpAddr(dest);
				printf(" but dist is %d\n",
				oroute->ort_dist));
			return;
		}
	}

	result= ipr_add_oroute(port_nr, dest, netmask, oroute->ort_gateway, 
		timeout, new_dist, FALSE, 0, NULL);
	assert(result == NW_OK);
}


PUBLIC int ipr_get_oroute(ent_no, route_ent)
int ent_no;
nwio_route_t *route_ent;
{
	oroute_t *oroute;

	if (ent_no<0 || ent_no>= OROUTE_NR)
		return ENOENT;

	oroute= &oroute_table[ent_no];
	if ((oroute->ort_flags & ORTF_INUSE) && oroute->ort_exp_tim &&
					oroute->ort_exp_tim < get_time())
	{
		oroute_del(oroute);
		oroute->ort_flags &= ~ORTF_INUSE;
	}

	route_ent->nwr_ent_no= ent_no;
	route_ent->nwr_ent_count= OROUTE_NR;
	route_ent->nwr_dest= oroute->ort_dest;
	route_ent->nwr_netmask= oroute->ort_subnetmask;
	route_ent->nwr_gateway= oroute->ort_gateway;
	route_ent->nwr_dist= oroute->ort_dist;
	route_ent->nwr_flags= NWRF_EMPTY;
	if (oroute->ort_flags & ORTF_INUSE)
	{
		route_ent->nwr_flags |= NWRF_INUSE;
		if (oroute->ort_flags & ORTF_STATIC)
			route_ent->nwr_flags |= NWRF_STATIC;
	}
	route_ent->nwr_pref= oroute->ort_pref;
	route_ent->nwr_ifaddr= ip_get_ifaddr(oroute->ort_port);
	return NW_OK;
}


PRIVATE oroute_t *oroute_find_ent(port_nr, dest)
int port_nr;
ipaddr_t dest;
{
	int hash, i, r_hash_ind;
	oroute_hash_t *oroute_hash;
	oroute_hash_t tmp_hash;
	oroute_t *oroute, *bestroute;
	time_t currtim;
	unsigned long hash_tmp;

	currtim= get_time();

	hash= hash_oroute(port_nr, dest, hash_tmp);
	oroute_hash= &oroute_hash_table[hash][0];
	if (oroute_hash[0].orh_addr == dest)
		oroute= oroute_hash[0].orh_route;
	else if (oroute_hash[1].orh_addr == dest)
	{
		tmp_hash= oroute_hash[1];
		oroute_hash[1]= oroute_hash[0];
		oroute_hash[0]= tmp_hash;
		oroute= tmp_hash.orh_route;
	}
	else if (oroute_hash[2].orh_addr == dest)
	{
		tmp_hash= oroute_hash[2];
		oroute_hash[2]= oroute_hash[1];
		oroute_hash[1]= oroute_hash[0];
		oroute_hash[0]= tmp_hash;
		oroute= tmp_hash.orh_route;
	}
	else if (oroute_hash[3].orh_addr == dest)
	{
		tmp_hash= oroute_hash[3];
		oroute_hash[3]= oroute_hash[2];
		oroute_hash[2]= oroute_hash[1];
		oroute_hash[1]= oroute_hash[0];
		oroute_hash[0]= tmp_hash;
		oroute= tmp_hash.orh_route;
	}
	else
		oroute= NULL;
	if (oroute)
	{
		assert(oroute->ort_port == port_nr);
		if (oroute->ort_exp_tim && oroute->ort_exp_tim<currtim)
		{
			oroute_del(oroute);
			oroute->ort_flags &= ~ORTF_INUSE;
		}
		else
			return oroute;
	}

	bestroute= NULL;
	for (oroute= oroute_head; oroute; oroute= oroute->ort_nextnw)
	{
		if (((dest ^ oroute->ort_dest) & oroute->ort_subnetmask) != 0)
			continue;
		if (oroute->ort_port != port_nr)
			continue;
		if (!bestroute)
		{
			bestroute= oroute;
			continue;
		}
		assert(oroute->ort_dest != bestroute->ort_dest);
		if (ntohl(oroute->ort_subnetmask) > 
			ntohl(bestroute->ort_subnetmask))
		{
			bestroute= oroute;
			continue;
		}
	}
	if (bestroute == NULL)
		return NULL;

	oroute_hash[3]= oroute_hash[2];
	oroute_hash[2]= oroute_hash[1];
	oroute_hash[1]= oroute_hash[0];
	oroute_hash[0].orh_addr= dest;
	oroute_hash[0].orh_route= bestroute;

	return bestroute;
}


PRIVATE void oroute_del(oroute)
oroute_t *oroute;
{
	oroute_t *prev, *nw_route, *gw_route, *dist_route, *prev_route;

	for (prev= NULL, nw_route= oroute_head; nw_route; 
				prev= nw_route, nw_route= nw_route->ort_nextnw)
	{
		if (oroute->ort_port == nw_route->ort_port &&
			oroute->ort_dest == nw_route->ort_dest &&
			oroute->ort_subnetmask == nw_route->ort_subnetmask)
		{
			break;
		}
	}
	assert(nw_route);
	if (prev)
		prev->ort_nextnw= nw_route->ort_nextnw;
	else
		oroute_head= nw_route->ort_nextnw;
	prev_route= nw_route;
	for (prev= NULL, gw_route= nw_route; gw_route; 
				prev= gw_route, gw_route= gw_route->ort_nextgw)
	{
		if (oroute->ort_gateway == gw_route->ort_gateway)
			break;
	}
	assert(gw_route);
	if (prev)
		prev->ort_nextgw= gw_route->ort_nextgw;
	else
		nw_route= gw_route->ort_nextgw;
	for (prev= NULL, dist_route= gw_route; dist_route; 
			prev= dist_route, dist_route= dist_route->ort_nextdist)
	{
		if (oroute == dist_route)
			break;
	}
	assert(dist_route);
	if (prev)
		prev->ort_nextdist= dist_route->ort_nextdist;
	else
		gw_route= dist_route->ort_nextdist;
	gw_route= sort_dists(gw_route);
	if (gw_route != NULL)
	{
		gw_route->ort_nextgw= nw_route;
		nw_route= gw_route;
	}
	nw_route= sort_gws(nw_route);
	if (nw_route != NULL)
	{
		nw_route->ort_nextnw= oroute_head;
		oroute_head= nw_route;
	}
	if (nw_route != prev_route)
	{
		oroute_uncache_nw(prev_route->ort_dest, 
			prev_route->ort_subnetmask);
	}
}


PRIVATE oroute_t *sort_dists(oroute)
oroute_t *oroute;
{
	oroute_t *r, *prev, *best, *best_prev;
	int best_dist, best_pref;

	best= NULL;
	for (prev= NULL, r= oroute; r; prev= r, r= r->ort_nextdist)
	{
		if (best == NULL)
			;	/* Force assignment to best */
		else if (r->ort_dist != best_dist)
		{
			if (r->ort_dist > best_dist)
				continue;
		}
		else
		{
			if (r->ort_pref <= best_pref)
				continue;
		}
		best= r;
		best_prev= prev;
		best_dist= r->ort_dist;
		best_pref= r->ort_pref;
	}
	if (!best)
	{
		assert(oroute == NULL);
		return oroute;
	}
	if (!best_prev)
	{
		assert(best == oroute);
		return oroute;
	}
	best_prev->ort_nextdist= best->ort_nextdist;
	best->ort_nextdist= oroute;
	return best;
}


PRIVATE oroute_t *sort_gws(oroute)
oroute_t *oroute;
{
	oroute_t *r, *prev, *best, *best_prev;
	int best_dist, best_pref;

	best= NULL;
	for (prev= NULL, r= oroute; r; prev= r, r= r->ort_nextgw)
	{
		if (best == NULL)
			;	/* Force assignment to best */
		else if (r->ort_dist != best_dist)
		{
			if (r->ort_dist > best_dist)
				continue;
		}
		else
		{
			if (r->ort_pref <= best_pref)
				continue;
		}
		best= r;
		best_prev= prev;
		best_dist= r->ort_dist;
		best_pref= r->ort_pref;
	}
	if (!best)
	{
		assert(oroute == NULL);
		return oroute;
	}
	if (!best_prev)
	{
		assert(best == oroute);
		return oroute;
	}
	best_prev->ort_nextgw= best->ort_nextgw;
	best->ort_nextgw= oroute;
	return best;
}


PRIVATE	oroute_uncache_nw(dest, netmask)
ipaddr_t dest;
ipaddr_t netmask;
{
	int i, j;
	oroute_hash_t *oroute_hash;

	for (i= 0, oroute_hash= &oroute_hash_table[0][0];
		i<OROUTE_HASH_NR; i++, oroute_hash += OROUTE_HASH_ASS_NR)
	{
		for (j= 0; j<OROUTE_HASH_ASS_NR; j++)
		{
			if (((oroute_hash[j].orh_addr ^ dest) & netmask) == 0)
			{
				oroute_hash[j].orh_addr= 0;
				oroute_hash[j].orh_route= NULL;
			}
		}
	}
}


/*
 * Input routing
 */

PUBLIC int ipr_get_iroute(ent_no, route_ent)
int ent_no;
nwio_route_t *route_ent;
{
	iroute_t *iroute;

	if (ent_no<0 || ent_no>= IROUTE_NR)
		return ENOENT;

	iroute= &iroute_table[ent_no];

	route_ent->nwr_ent_count= IROUTE_NR;
	route_ent->nwr_dest= iroute->irt_dest;
	route_ent->nwr_netmask= iroute->irt_subnetmask;
	route_ent->nwr_gateway= iroute->irt_gateway;
	route_ent->nwr_dist= iroute->irt_dist;
	route_ent->nwr_flags= NWRF_EMPTY;
	if (iroute->irt_flags & IRTF_INUSE)
	{
		route_ent->nwr_flags |= NWRF_INUSE;
		if (iroute->irt_flags & IRTF_STATIC)
			route_ent->nwr_flags |= NWRF_STATIC;
		if (iroute->irt_dist == IRTD_UNREACHABLE)
			route_ent->nwr_flags |= NWRF_UNREACHABLE;
	}
	route_ent->nwr_pref= 0;
	route_ent->nwr_ifaddr= ip_get_ifaddr(iroute->irt_port);
	return NW_OK;
}


PUBLIC int ipr_add_iroute(port_nr, dest, subnetmask, gateway, 
	dist, static_route, iroute_p)
int port_nr;
ipaddr_t dest;
ipaddr_t subnetmask;
ipaddr_t gateway;
int dist;
int static_route;
iroute_t **iroute_p;
{
	int i;
	iroute_t *iroute, *unused_route;

	unused_route= NULL;
	if (static_route)
	{
		/* Static routes are not reused automatically, so we look 
		 * for an unused entry.
		 */
		for(i= 0, iroute= iroute_table; i<IROUTE_NR; i++, iroute++)
		{
			if ((iroute->irt_flags & IRTF_INUSE) == 0)
				break;
		}
		if (i != IROUTE_NR)
			unused_route= iroute;
	}
	else
	{
		/* Try to track down any old routes, and look for an
		 * unused one.
		 */
		for(i= 0, iroute= iroute_table; i<IROUTE_NR; i++, iroute++)
		{
			if ((iroute->irt_flags & IRTF_INUSE) == 0)
			{
				unused_route= iroute;
				continue;
			}
			if ((iroute->irt_flags & IRTF_STATIC) != 0)
				continue;
			if (iroute->irt_port != port_nr ||
				iroute->irt_dest != dest ||
				iroute->irt_subnetmask != subnetmask ||
				iroute->irt_gateway != gateway)
			{
				continue;
			}
			break;
		}
		if (i != IROUTE_NR)
			unused_route= iroute;
	}

	if (unused_route == NULL)
		return ENOMEM;
	iroute= unused_route;

	iroute->irt_port= port_nr;
	iroute->irt_dest= dest;
	iroute->irt_subnetmask= subnetmask;
	iroute->irt_gateway= gateway;
	iroute->irt_dist= dist;
	iroute->irt_flags= IRTF_INUSE;
	if (static_route)
		iroute->irt_flags |= IRTF_STATIC;
	
	iroute_uncache_nw(iroute->irt_dest, iroute->irt_subnetmask);
	if (iroute_p != NULL)
		*iroute_p= iroute;
	return NW_OK;
}


PUBLIC int ipr_del_iroute(port_nr, dest, subnetmask, gateway, 
	dist, static_route)
int port_nr;
ipaddr_t dest;
ipaddr_t subnetmask;
ipaddr_t gateway;
int dist;
int static_route;
{
	int i;
	iroute_t *iroute;

	/* Try to track down any old routes, and look for an
	 * unused one.
	 */
	for(i= 0, iroute= iroute_table; i<IROUTE_NR; i++, iroute++)
	{
		if ((iroute->irt_flags & IRTF_INUSE) == 0)
			continue;
		if (iroute->irt_port != port_nr ||
			iroute->irt_dest != dest ||
			iroute->irt_subnetmask != subnetmask ||
			iroute->irt_gateway != gateway)
		{
			continue;
		}
		if (!!(iroute->irt_flags & IRTF_STATIC) != static_route)
			continue;
		break;
	}

	if (i == IROUTE_NR)
		return ESRCH;

	iroute_uncache_nw(iroute->irt_dest, iroute->irt_subnetmask);
	iroute->irt_flags= IRTF_EMPTY;
	return NW_OK;
}


PRIVATE	iroute_uncache_nw(dest, netmask)
ipaddr_t dest;
ipaddr_t netmask;
{
	int i, j;
	iroute_hash_t *iroute_hash;

	for (i= 0, iroute_hash= &iroute_hash_table[0][0];
		i<IROUTE_HASH_NR; i++, iroute_hash += IROUTE_HASH_ASS_NR)
	{
		for (j= 0; j<IROUTE_HASH_ASS_NR; j++)
		{
			if (((iroute_hash[j].irh_addr ^ dest) &
				netmask) == 0)
			{
				iroute_hash[j].irh_addr= 0;
				iroute_hash[j].irh_route= NULL;
			}
		}
	}
}



/*
 * Debugging, management
 */

/*
 * $PchId: ipr.c,v 1.9 1996/07/31 17:26:33 philip Exp $
 */
