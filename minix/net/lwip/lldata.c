/* LWIP service - lldata.c - link-layer (ARP, NDP) data related routines */
/*
 * This module is largely isolated from the regular routing code.  There are
 * two reasons for that.  First, mixing link-layer routes with regular routes
 * would not work well due to the fact that lwIP keeps these data structures
 * entirely separate.  Second, as of version 8, NetBSD keeps the IP-layer and
 * link-layer routing separate as well.
 *
 * Unfortunately, lwIP does not provide much in the way of implementing the
 * functionality that would be expected for this module.  As such, the current
 * implementation is very restricted and simple.
 *
 * For ARP table entries, lwIP only allows for adding and deleting static
 * entries.  Non-static entries cannot be deleted.  Incomplete (pending)
 * entries cannot even be enumerated, nor can (e.g.) expiry information be
 * obtained.  The lwIP ARP datastructures are completely hidden, so there is no
 * way to overcome these limitations without changing lwIP itself.  As a
 * result, not all functionality of the arp(8) userland utility is supported.
 *
 * For NDP table entries, lwIP offers no API at all.  However, since the data
 * structures are exposed directly, we can use those to implement full support
 * for exposing information in a read-only way.  However, manipulating data
 * structures directly from here is too risky, nor does lwIP currently support
 * the concept of static NDP table entries.  Therefore, adding, changing, and
 * deleting NDP entries is currently not supported, and will also first require
 * changes to lwIP itself.
 *
 * The ndp(8) userland utility is also able to show and manipulate various
 * other neighbor discovery related tables and settings.  We support only a
 * small subset of them.  The main reason for this is that the other tables,
 * in particular the prefix and default router lists, are not relevant: on
 * MINIX 3, these are always managed fully in userland (usually dhcpcd(8)), and
 * we even hardcode lwIP not to parse Router Advertisement messages at all, so
 * even though those tables are still part of lwIP, they are always empty.
 * Other ndp(8) functionality are unsupported for similar reasons.
 */

#include "lwip.h"
#include "lldata.h"
#include "route.h"
#include "rtsock.h"

#include "lwip/etharp.h"
#include "lwip/nd6.h"
#include "lwip/priv/nd6_priv.h" /* for neighbor_cache */

/*
 * Process a routing command specifically for an ARP table entry.  Return OK if
 * the routing command has been processed successfully and a routing socket
 * reply message has already been generated.  Return a negative error code on
 * failure, in which case the caller will generate a reply message instead.
 */
static int
lldata_arp_process(unsigned int type, const ip_addr_t * dst_addr,
	const struct eth_addr * gw_addr, struct ifdev * ifdev,
	unsigned int flags, const struct rtsock_request * rtr)
{
	const ip4_addr_t *ip4addr;
	struct eth_addr ethaddr, *ethptr;
	struct netif *netif;
	lldata_arp_num_t num;
	err_t err;

	netif = (ifdev != NULL) ? ifdev_get_netif(ifdev) : NULL;

	num = etharp_find_addr(netif, ip_2_ip4(dst_addr), &ethptr, &ip4addr);

	if (type != RTM_ADD && num < 0)
		return ESRCH;
	else if (type == RTM_ADD && num >= 0)
		return EEXIST;

	switch (type) {
	case RTM_CHANGE:
		/*
		 * This request is not used by arp(8), so keep things simple.
		 * For RTM_ADD we support only static entries; we support only
		 * those too here, and thus we can use delete-and-readd.  If
		 * the ethernet address is not being changed, try readding the
		 * entry with the previous ethernet address.
		 */
		if (gw_addr == NULL)
			gw_addr = ethptr;

		if (etharp_remove_static_entry(ip_2_ip4(dst_addr)) != ERR_OK)
			return EPERM;

		/* FALLTHROUGH */
	case RTM_ADD:
		assert(gw_addr != NULL);

		memcpy(&ethaddr, gw_addr, sizeof(ethaddr));

		/*
		 * Adding static, permanent, unpublished, non-proxy entries is
		 * all that lwIP supports right now.  We also do not get to
		 * specify the interface, and the way lwIP picks the interface
		 * may in fact result in a different one.
		 */
		if ((err = etharp_add_static_entry(ip_2_ip4(dst_addr),
		    &ethaddr)) != ERR_OK)
			return util_convert_err(err);

		if ((num = etharp_find_addr(NULL /*netif*/, ip_2_ip4(dst_addr),
		    &ethptr, &ip4addr)) < 0)
			panic("unable to find just-added static ARP entry");

		/* FALLTHROUGH */
	case RTM_LOCK:
	case RTM_GET:
		rtsock_msg_arp(num, type, rtr);

		return OK;

	case RTM_DELETE:
		memcpy(&ethaddr, ethptr, sizeof(ethaddr));

		if (etharp_remove_static_entry(ip_2_ip4(dst_addr)) != ERR_OK)
			return EPERM;

		/*
		 * FIXME: the following block is a hack, because we cannot
		 * predict whether the above removal will succeed, while at the
		 * same time we need the entry to be present in order to report
		 * the deleted address to the routing socket.  We temporarily
		 * readd and then remove the entry just for the purpose of
		 * generating the routing socket reply.  There are other ways
		 * to resolve this, but only a better lwIP etharp API would
		 * allow us to resolve this problem cleanly.
		 */
		(void)etharp_add_static_entry(ip_2_ip4(dst_addr), &ethaddr);

		num = etharp_find_addr(NULL /*netif*/, ip_2_ip4(dst_addr),
		    &ethptr, &ip4addr);
		assert(num >= 0);

		rtsock_msg_arp(num, type, rtr);

		(void)etharp_remove_static_entry(ip_2_ip4(dst_addr));

		return OK;

	default:
		return EINVAL;
	}
}

/*
 * Enumerate ARP table entries.  Return TRUE if there is at least one more ARP
 * table entry, of which the number is stored in 'num'.  The caller should set
 * 'num' to 0 initially, and increase it by one between a successful call and
 * the next call.  Return FALSE if there are no more ARP table entries.
 */
int
lldata_arp_enum(lldata_arp_num_t * num)
{
	ip4_addr_t *ip4addr;
	struct netif *netif;
	struct eth_addr *ethaddr;

	for (; *num < ARP_TABLE_SIZE; ++*num) {
		if (etharp_get_entry(*num, &ip4addr, &netif, &ethaddr))
			return TRUE;
	}

	return FALSE;
}

/*
 * Obtain information about the ARP table entry identified by 'num'.  The IPv4
 * address of the entry is stored in 'addr'.  Its ethernet address is stored in
 * 'gateway'.  The associated interface is stored in 'ifdevp', and the entry's
 * routing flags (RTF_) are stored in 'flagsp'.
 */
void
lldata_arp_get(lldata_arp_num_t num, struct sockaddr_in * addr,
	struct sockaddr_dlx * gateway, struct ifdev ** ifdevp,
	unsigned int * flagsp)
{
	ip_addr_t ipaddr;
	ip4_addr_t *ip4addr;
	struct netif *netif;
	struct ifdev *ifdev;
	struct eth_addr *ethaddr;
	socklen_t addr_len;

	if (!etharp_get_entry(num, &ip4addr, &netif, &ethaddr))
		panic("request for invalid ARP entry");

	ip_addr_copy_from_ip4(ipaddr, *ip4addr);

	assert(netif != NULL);
	ifdev = netif_get_ifdev(netif);

	addr_len = sizeof(*addr);

	addr_put_inet((struct sockaddr *)addr, &addr_len, &ipaddr,
	    TRUE /*kame*/, 0 /*port*/);

	addr_len = sizeof(*gateway);

	addr_put_link((struct sockaddr *)gateway, &addr_len,
	    ifdev_get_index(ifdev), ifdev_get_iftype(ifdev), NULL /*name*/,
	    ethaddr->addr, sizeof(ethaddr->addr));

	*ifdevp = ifdev;

	/*
	 * TODO: this is not necessarily accurate, but lwIP does not provide us
	 * with information as to whether this is a static entry or not..
	 */
	*flagsp = RTF_HOST | RTF_LLINFO | RTF_LLDATA | RTF_STATIC | RTF_CLONED;
}

/*
 * Obtain information about the ND6 neighbor cache entry 'i', which must be a
 * number between 0 (inclusive) and LWIP_ND6_NUM_NEIGHBORS (exclusive).  If an
 * entry with this number exists, return a pointer to its IPv6 address, and
 * additional information in each of the given pointers if not NULL.  The
 * associated interface is stored in 'netif'.  If the entry has an associated
 * link-layer address, a pointer to it is stored in 'lladdr'.  The entry's
 * state (ND6_{INCOMPLETE,REACHABLE,STALE,DELAY,PROBE}) is stored in 'state'.
 * The 'isrouter' parameter is filled with a boolean value indicating whether
 * the entry is for a router.  For ND6_INCOMPLETE and ND6_PROBE, the number of
 * probes sent so far is stored in 'probes_sent'; for other states, the value
 * is set to zero.  For ND6_REACHABLE and ND6_DELAY, the time until expiration
 * in ND6_TMR_INTERVAL-millisecond units is stored in 'expire_time'; for other
 * states, the value is set to zero.  If an entry with number 'i' does not
 * exist, NULL is returned.
 *
 * TODO: upstream this function to lwIP.
 */
static const ip6_addr_t *
nd6_get_neighbor_cache_entry(int8_t i, struct netif ** netif,
	const uint8_t ** lladdr, uint8_t * state, uint8_t * isrouter,
	uint32_t * probes_sent, uint32_t * expire_time)
{

	if (i < 0 || i >= LWIP_ND6_NUM_NEIGHBORS ||
	    neighbor_cache[i].state == ND6_NO_ENTRY)
		return NULL;

	if (netif != NULL)
		*netif = neighbor_cache[i].netif;

	if (lladdr != NULL) {
		if (neighbor_cache[i].state != ND6_INCOMPLETE)
			*lladdr = neighbor_cache[i].lladdr;
		else
			*lladdr = NULL;
	}

	if (state != NULL)
		*state = neighbor_cache[i].state;

	if (isrouter != NULL)
		*isrouter = neighbor_cache[i].isrouter;

	if (probes_sent != NULL) {
		if (neighbor_cache[i].state == ND6_INCOMPLETE ||
		    neighbor_cache[i].state == ND6_PROBE)
			*probes_sent = neighbor_cache[i].counter.probes_sent;
		else
			*probes_sent = 0;
	}

	if (expire_time != NULL) {
		switch (neighbor_cache[i].state) {
		case ND6_REACHABLE:
			*expire_time =
			    neighbor_cache[i].counter.reachable_time /
			    ND6_TMR_INTERVAL;
			break;
		case ND6_DELAY:
			*expire_time = neighbor_cache[i].counter.delay_time;
			break;
		case ND6_INCOMPLETE:
		case ND6_PROBE:
			/* Probes are sent once per timer tick. */
			*expire_time = (LWIP_ND6_MAX_MULTICAST_SOLICIT + 1 -
			    neighbor_cache[i].counter.probes_sent) *
			    (ND6_TMR_INTERVAL / 1000);
			break;
		default:
			/* Stale entries do not expire; they get replaced. */
			*expire_time = 0;
			break;
		}
	}

	return &neighbor_cache[i].next_hop_address;
}

/*
 * Find a neighbor cache entry by IPv6 address.  Return its index number if
 * found, or -1 if not.  This is a reimplementation of the exact same function
 * internal to lwIP.
 *
 * TODO: make this function public in lwIP.
 */
static int8_t
nd6_find_neighbor_cache_entry(const ip6_addr_t * addr)
{
	int8_t i;

	for (i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
		if (ip6_addr_cmp(addr, &neighbor_cache[i].next_hop_address))
			return i;
	}

	return -1;
}

/*
 * Find an NDP table entry based on the given interface and IPv6 address.  On
 * success, return OK, with the entry's index number stored in 'nump'.  On
 * failure, return an appropriate error code.
 */
int
lldata_ndp_find(struct ifdev * ifdev, const struct sockaddr_in6 * addr,
	lldata_ndp_num_t * nump)
{
	ip_addr_t ipaddr;
	int8_t i;
	int r;

	if ((r = addr_get_inet((const struct sockaddr *)addr, sizeof(*addr),
	    IPADDR_TYPE_V6, &ipaddr, TRUE /*kame*/, NULL /*port*/)) != OK)
		return r;

	/*
	 * For given link-local addresses, no zone may be provided in the
	 * address at all.  In such cases, add the zone ourselves, using the
	 * given interface.
	 */
	if (ip6_addr_lacks_zone(ip_2_ip6(&ipaddr), IP6_UNKNOWN))
		ip6_addr_assign_zone(ip_2_ip6(&ipaddr), IP6_UNKNOWN,
		    ifdev_get_netif(ifdev));

	i = nd6_find_neighbor_cache_entry(ip_2_ip6(&ipaddr));
	if (i < 0)
		return ESRCH;

	/*
	 * We should compare the neighbor cache entry's associated netif to
	 * the given ifdev, but since the lwIP neighbor cache is currently not
	 * keyed by netif anyway (i.e. the internal lookups are purely by IPv6
	 * address as well), doing so makes little sense in practice.
	 */

	*nump = (lldata_ndp_num_t)i;
	return OK;
}

/*
 * Process a routing command specifically for an NDP table entry.  Return OK if
 * the routing command has been processed successfully and a routing socket
 * reply message has already been generated.  Return a negative error code on
 * failure, in which case the caller will generate a reply message instead.
 */
static int
lldata_ndp_process(unsigned int type, const ip_addr_t * dst_addr,
	const struct eth_addr * gw_addr,
	struct ifdev * ifdev, unsigned int flags,
	const struct rtsock_request * rtr)
{
	lldata_ndp_num_t num;

	num = (lldata_ndp_num_t)
	    nd6_find_neighbor_cache_entry(ip_2_ip6(dst_addr));

	if (type != RTM_ADD && num < 0)
		return ESRCH;
	else if (type == RTM_ADD && num >= 0)
		return EEXIST;

	switch (type) {
	case RTM_LOCK:
	case RTM_GET:
		rtsock_msg_arp(num, type, rtr);

		return OK;

	case RTM_ADD:
	case RTM_CHANGE:
	case RTM_DELETE:
		/* TODO: add lwIP support to implement these commands. */
		return ENOSYS;

	default:
		return EINVAL;
	}
}

/*
 * Enumerate NDP table entries.  Return TRUE if there is at least one more NDP
 * table entry, of which the number is stored in 'num'.  The caller should set
 * 'num' to 0 initially, and increase it by one between a successful call and
 * the next call.  Return FALSE if there are no more NDP table entries.
 */
int
lldata_ndp_enum(lldata_ndp_num_t * num)
{

	for (; *num < LWIP_ND6_NUM_NEIGHBORS; ++*num) {
		if (nd6_get_neighbor_cache_entry(*num, NULL /*netif*/,
		    NULL /*lladdr*/, NULL /*state*/, NULL /*isrouter*/,
		    NULL /*probes_sent*/, NULL /*expire_time*/) != NULL)
			return TRUE;
	}

	return FALSE;
}

/*
 * Obtain information about the NDP table entry identified by 'num'.  The IPv6
 * address of the entry is stored in 'addr'.  Its ethernet address is stored in
 * 'gateway'.  The associated interface is stored in 'ifdevp', and the entry's
 * routing flags (RTF_) are stored in 'flagsp'.
 */
void
lldata_ndp_get(lldata_ndp_num_t num, struct sockaddr_in6 * addr,
	struct sockaddr_dlx * gateway, struct ifdev ** ifdevp,
	unsigned int * flagsp)
{
	const ip6_addr_t *ip6addr;
	ip_addr_t ipaddr;
	struct netif *netif = NULL /*gcc*/;
	struct ifdev *ifdev;
	const uint8_t *lladdr = NULL /*gcc*/;
	socklen_t addr_len;

	ip6addr = nd6_get_neighbor_cache_entry(num, &netif, &lladdr,
	    NULL /*state*/, NULL /*isrouter*/, NULL /*probes_sent*/,
	    NULL /*expire_time*/);
	assert(ip6addr != NULL);

	ip_addr_copy_from_ip6(ipaddr, *ip6addr);

	ifdev = netif_get_ifdev(netif);
	assert(ifdev != NULL);

	addr_len = sizeof(*addr);

	addr_put_inet((struct sockaddr *)addr, &addr_len, &ipaddr,
	    TRUE /*kame*/, 0 /*port*/);

	addr_len = sizeof(*gateway);

	addr_put_link((struct sockaddr *)gateway, &addr_len,
	    ifdev_get_index(ifdev), ifdev_get_iftype(ifdev), NULL /*name*/,
	    lladdr, ifdev_get_hwlen(ifdev));

	*ifdevp = ifdev;
	*flagsp = RTF_HOST | RTF_LLINFO | RTF_LLDATA | RTF_CLONED;
}

/*
 * Obtain information about the NDP table entry with the number 'num', which
 * must be obtained through a previous call to lldata_ndp_find().  On return,
 * 'asked' is filled with the number of probes sent so far (0 if inapplicable),
 * 'isrouter' is set to 1 or 0 depending on whether the entry is for a router,
 * 'state' is set to the entry's state (ND6_LLINFO_), and 'expire' is set to
 * either the UNIX timestamp of expiry for the entry; 0 for permanent entries.
 * None of the given pointers must be NULL.  This function always succeeds.
 */
void
lldata_ndp_get_info(lldata_ndp_num_t num, long * asked, int * isrouter,
	int * state, int * expire)
{
	uint32_t nd6_probes_sent = 0 /*gcc*/, nd6_expire_time = 0 /*gcc*/;
	uint8_t nd6_state = 0 /*gcc*/, nd6_isrouter = 0 /*gcc*/;

	(void)nd6_get_neighbor_cache_entry(num, NULL /*netif*/,
	    NULL /*lladdr*/, &nd6_state, &nd6_isrouter, &nd6_probes_sent,
	    &nd6_expire_time);

	*asked = (long)nd6_probes_sent;

	*isrouter = !!nd6_isrouter;

	switch (nd6_state) {
	case ND6_INCOMPLETE:	*state = ND6_LLINFO_INCOMPLETE; break;
	case ND6_REACHABLE:	*state = ND6_LLINFO_REACHABLE; break;
	case ND6_STALE:		*state = ND6_LLINFO_STALE; break;
	case ND6_DELAY:		*state = ND6_LLINFO_DELAY; break;
	case ND6_PROBE:		*state = ND6_LLINFO_PROBE; break;
	default:		panic("unknown ND6 state %u", nd6_state);
	}

	if (nd6_expire_time != 0)
		*expire = clock_time(NULL) +
		    (int)nd6_expire_time * (ND6_TMR_INTERVAL / 1000);
	else
		*expire = 0;
}

/*
 * Process a routing command specifically for a link-layer route, as one of the
 * specific continuations of processing started by route_process().  The RTM_
 * routing command is given as 'type'.  The route destination is given as
 * 'dst_addr'; its address type determines whether the operation is for ARP or
 * NDP.  The sockaddr structure for 'gateway' is passed on as is and may have
 * to be parsed here if not NULL.  'ifdev' is the interface to be associated
 * with the route; it is non-NULL only if an interface name (IFP) or address
 * (IFA) was given.  The RTF_ flags field has been checked against the globally
 * supported flags, but may have to be checked for flags that do not apply to
 * ARP/NDP routes.  Return OK or a negative error code, following the same
 * semantics as route_process().
 */
int
lldata_process(unsigned int type, const ip_addr_t * dst_addr,
	const struct sockaddr * gateway, struct ifdev * ifdev,
	unsigned int flags, const struct rtsock_request * rtr)
{
	const struct route_entry *route;
	struct eth_addr ethaddr, *gw_addr;
	int r;

	assert(flags & RTF_LLDATA);

	/*
	 * It seems that RTF_UP does not apply to link-layer routing entries.
	 * We basically accept any flags that we can return, but we do not
	 * actually check most of them anywhere.
	 */
	if ((flags & ~(RTF_HOST | RTF_LLINFO | RTF_LLDATA | RTF_STATIC |
	   RTF_CLONED | RTF_ANNOUNCE)) != 0)
		return EINVAL;

	gw_addr = NULL;

	if (type == RTM_ADD || type == RTM_CHANGE) {
		/*
		 * Link-layer entries are always host entries.  Not all
		 * requests pass in this flag though, so check only when the
		 * flags are supposed to be set.
		 */
		if ((type == RTM_ADD || type == RTM_CHANGE) &&
		    !(flags & RTF_HOST))
			return EINVAL;

		/* lwIP does not support publishing custom entries. */
		if (flags & RTF_ANNOUNCE)
			return ENOSYS;

		/* RTF_GATEWAY is always cleared for link-layer entries. */
		if (gateway != NULL) {
			if ((r = addr_get_link(gateway, gateway->sa_len,
			    NULL /*name*/, 0 /*name_max*/, ethaddr.addr,
			    sizeof(ethaddr.addr))) != OK)
				return r;

			gw_addr = &ethaddr;
		}

		if (type == RTM_ADD) {
			if (gateway == NULL)
				return EINVAL;

			/*
			 * If no interface has been specified, see if the
			 * destination address is on a locally connected
			 * network.  If so, use that network's interface.
			 * Otherwise reject the request altogether: we must
			 * have an interface to which to associate the entry.
			 */
			if (ifdev == NULL) {
				if ((route = route_lookup(dst_addr)) != NULL &&
				    !(route_get_flags(route) & RTF_GATEWAY))
					ifdev = route_get_ifdev(route);
				else
					return ENETUNREACH;
			}
		}
	}

	if (IP_IS_V4(dst_addr))
		return lldata_arp_process(type, dst_addr, gw_addr, ifdev,
		    flags, rtr);
	else
		return lldata_ndp_process(type, dst_addr, gw_addr, ifdev,
		    flags, rtr);
}
