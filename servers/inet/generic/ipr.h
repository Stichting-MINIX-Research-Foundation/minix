/*
ipr.h

Copyright 1995 Philip Homburg
*/

#ifndef IPR_H
#define IPR_H

typedef struct oroute
{
	int ort_port;
	ipaddr_t ort_dest;
	ipaddr_t ort_subnetmask;
	int ort_dist;
	i32_t ort_pref;
	ipaddr_t ort_gateway;
	time_t ort_exp_tim;
	time_t ort_timestamp;
	int ort_flags;

	struct oroute *ort_nextnw;
	struct oroute *ort_nextgw;
	struct oroute *ort_nextdist;
} oroute_t;

#define ORTD_UNREACHABLE	512

#define ORTF_EMPTY		0
#define ORTF_INUSE		1
#define ORTF_STATIC		2

typedef struct iroute
{
	ipaddr_t irt_dest;
	ipaddr_t irt_gateway;
	ipaddr_t irt_subnetmask;
	int irt_dist;
	int irt_port;
	int irt_flags;
} iroute_t;

#define IRTD_UNREACHABLE	512

#define IRTF_EMPTY		0
#define IRTF_INUSE		1
#define IRTF_STATIC		2

#define IPR_UNRCH_TIMEOUT	(60L * HZ)
#define IPR_TTL_TIMEOUT		(60L * HZ)
#define IPR_REDIRECT_TIMEOUT	(20 * 60L * HZ)
#define IPR_GW_DOWN_TIMEOUT	(60L * HZ)

/* Prototypes */

iroute_t *iroute_frag ARGS(( int port_nr, ipaddr_t dest ));
int oroute_frag ARGS(( int port_nr, ipaddr_t dest, int ttl, 
							ipaddr_t *nexthop ));
void ipr_init ARGS(( void ));
int ipr_get_iroute ARGS(( int ent_no, nwio_route_t *route_ent ));
int ipr_add_iroute ARGS(( int port_nr, ipaddr_t dest, ipaddr_t subnetmask, 
	ipaddr_t gateway, int dist, int static_route, iroute_t **route_p ));
int ipr_del_iroute ARGS(( int port_nr, ipaddr_t dest, ipaddr_t subnetmask, 
	ipaddr_t gateway, int dist, int static_route ));
int ipr_get_oroute ARGS(( int ent_no, nwio_route_t *route_ent ));
int ipr_add_oroute ARGS(( int port_nr, ipaddr_t dest, ipaddr_t subnetmask, 
	ipaddr_t gateway, time_t timeout, int dist, int static_route,
	i32_t preference, oroute_t **route_p ));
void ipr_gateway_down ARGS(( int port_nr, ipaddr_t gateway, time_t timeout ));
void ipr_redirect ARGS(( int port_nr, ipaddr_t dest, ipaddr_t subnetmask,
	ipaddr_t old_gateway, ipaddr_t new_gateway, time_t timeout ));
void ipr_destunrch ARGS(( int port_nr, ipaddr_t dest, ipaddr_t subnetmask,
	time_t timeout ));
void ipr_ttl_exc ARGS(( int port_nr, ipaddr_t dest, ipaddr_t subnetmask,
	time_t timeout ));

#endif /* IPR_H */

/*
 * $PchId: ipr.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
