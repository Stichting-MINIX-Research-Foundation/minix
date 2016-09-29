/* LWIP service - route.c - route management */
/*
 * This module provides a destination-based routing implementation, roughly
 * matching the routing as done traditionally by the BSDs and by current NetBSD
 * in particular.  As such, this implementation almost completely replaces
 * lwIP's own more limited (and less rigid) routing algorithms.  It does this
 * using a combination of overriding lwIP functions (ip4_route, ip6_route) with
 * weak-symbol patching, and lwIP-provided gateway hooks.  Especially the
 * former gives us a level of control that lwIP's routing hooks do not provide:
 * not only does such overriding give us the ability to flag that no route was
 * found at all, we also bypass a number of default decisions taken by lwIP
 * where the routing hooks are not called at all.
 *
 * As a result, the routing tables as visible to the user are an almost
 * completely accurate reflection of the routing decisions taken by this TCP/IP
 * stack in practice.  There is currently only one exception: for IPv4 gateway
 * selection, lwIP will bypass the gateway hook if the given address is on the
 * local subnet according to the locally assigned IP address and subnet mask.
 * This exception should practically affect noone, though.
 *
 * Our routing implementation differs from NetBSD's in various aspects, though.
 * Perhaps the most important one, also noted elsewhere, is that we do not
 * support the coexistence of an all-bits-set network route and a host route
 * for the same IP address.  If necessary, this issue can be resolved.
 *
 * We use a custom concept of "immutable" routes for local addresses, which are
 * a somewhat special case as explained in the ifaddr module.  Since those
 * RTF_LOCAL routes cannot be deleted, a small change is made to the route(8)
 * flush-all command to skip them.  Packets directed at local addresses on
 * non-loopback interfaces are handled in a way that differs from NetBSD's,
 * too.  This is explained in the ifdev module.
 *
 * The BSDs support special routes that reject or blackhole packets, based on
 * routing flags.  We support such routes as well, but implement them somewhat
 * differently from the BSDs: such packets always get routed over a loopback
 * interface (regardless of their associated interface), in order to save on
 * routing lookups for packets in the common case.
 *
 * As general rules of thumb: if there is no route to a destination, assignment
 * of a local address will already fail with a "no route to host" error.  If
 * there is an RTF_REJECT route, a local address will be assigned, but actual
 * packets will be routed to a loopback interface and result in a "no route to
 * host" error upon reception there - this is what NetBSD seems to do too, even
 * though the documentation says that RTF_REJECT routes generate ICMP messages
 * instead.  RTF_BLACKHOLE behaves similarly to RTF_REJECT, except that the
 * packet is simply discarded upon receipt by the loopback interface.
 *
 * In various places, both here and elsewhere, we check to make sure that on
 * routing and output, scoped IPv6 source and destination addresses never leave
 * their zone.  For example, a packet must not be sent to an outgoing interface
 * if its source address is a link-local address with a zone for another
 * interface.  lwIP does not check for such violations, and so we must make
 * sure that this does not happen ourselves.
 *
 * Normally, one would tell lwIP to use a particular default IPv4 gateway by
 * associating the gateway address to a particular interface, and then setting
 * that interface as default interface (netif_default).  We explicitly do
 * neither of these things.  Instead, the routing hooks should return the
 * default route whenever applicable, and the gateway hooks should return the
 * default route's gateway IP address whenever needed.
 *
 * Due to lwIP's limited set of error codes, we do not properly distinguish
 * between cases where EHOSTUNREACH or ENETUNREACH should be thrown, and throw
 * the former in most cases.
 */

#include "lwip.h"
#include "ifaddr.h"
#include "rttree.h"
#include "rtsock.h"
#include "route.h"
#include "lldata.h"

#include "lwip/nd6.h"

/*
 * The maximum number of uint8_t bytes needed to represent a routing address.
 * This value is the maximum of 4 (for IPv4) and 16 (for IPv6).
 */
#define ROUTE_ADDR_MAX	(MAX(IP4_BITS, IP6_BITS) / NBBY)

/*
 * We use a shared routing entry data structure for IPv4 and IPv6 routing
 * entries.  The result is cleaner code at the cost of (currently) about 2.3KB
 * of memory wasted (costing 12 bytes per address for three addresses for 64 of
 * the 128 routing entries that would be for IPv4), although with the benefit
 * that either address family may use more than half of the routing entries.
 * From that 2.3KB, 1KB can be reclaimed by moving the destination address and
 * mask into the rttree_entry data structure, at the cost of its generality.
 */
struct route_entry {
	struct rttree_entry re_entry;		/* routing tree entry */
	union pxfer_re_pu {
		struct ifdev *repu_ifdev;	/* associated interface */
		SIMPLEQ_ENTRY(route_entry) repu_next;	/* next free pointer */
	} re_pu;
	unsigned int re_flags;			/* routing flags (RTF_) */
	unsigned int re_use;			/* number of times used */
	uint8_t re_addr[ROUTE_ADDR_MAX];	/* destination address */
	uint8_t re_mask[ROUTE_ADDR_MAX];	/* destination mask */
	union ixfer_re_gu {
		ip4_addr_p_t regu_gw4;		/* gateway (IPv4) */
		ip6_addr_p_t regu_gw6;		/* gateway (IPv6) */
	} re_gu;
};
#define re_ifdev	re_pu.repu_ifdev
#define re_next		re_pu.repu_next
#define re_gw4		re_gu.regu_gw4
#define re_gw6		re_gu.regu_gw6

/* Routes for local addresses are immutable, for reasons explained in ifdev. */
#define route_is_immutable(route)	((route)->re_flags & RTF_LOCAL)

/*
 * We override a subset of the BSD routing flags in order to store our own
 * local settings.  In particular, we have to have a way to store whether a
 * route is for an IPv4 or IPv6 destination address.  We override BSD's
 * RTF_DONE flag for this: RTF_DONE is only used with routing sockets, and
 * never associated with actual routes.  In contrast, RTF_IPV6 is only used
 * with actual routes, and never sent across routing sockets.  In general,
 * overriding flags is preferable to adding new ones, as BSD might later add
 * more flags itself as well, while it can never remove existing flags.
 */
#define RTF_IPV6	RTF_DONE	/* route is for an IPv6 destination */

/* The total number of routing entries (IPv4 and IPv6 combined). */
#define NR_ROUTE_ENTRY	128

static struct route_entry route_array[NR_ROUTE_ENTRY];	/* routing entries */

static SIMPLEQ_HEAD(, route_entry) route_freelist;	/* free entry list */

/* The routing trees.  There are two: one for IPv4 and one for IPv6. */
#define ROUTE_TREE_V4	0
#define ROUTE_TREE_V6	1
#define NR_ROUTE_TREE	2

static struct rttree route_tree[NR_ROUTE_TREE];

/* We support a single cached routing entry per address family (IPv4, IPv6). */
static int rtcache_v4set;
static ip4_addr_t rtcache_v4addr;
static struct route_entry *rtcache_v4route;

static int rtcache_v6set;
static ip6_addr_t rtcache_v6addr;
static struct route_entry *rtcache_v6route;

/*
 * Initialize the routing cache.  There are a lot of trivial functions here,
 * but this is designed to be extended in the future.
 */
static void
rtcache_init(void)
{

	rtcache_v4set = FALSE;
	rtcache_v6set = FALSE;
}

/*
 * Look up the given IPv4 address in the routing cache.  If there is a match,
 * return TRUE with the associated route in 'route', possibly NULL if a
 * negative result was cached.  Return FALSE if the routing cache does not
 * cache the given IPv4 address.
 */
static inline int
rtcache_lookup_v4(const ip4_addr_t * ipaddr, struct route_entry ** route)
{

	if (rtcache_v4set && ip4_addr_cmp(&rtcache_v4addr, ipaddr)) {
		*route = rtcache_v4route;

		return TRUE;
	} else
		return FALSE;
}

/*
 * Add the given IPv4 address and the given routing entry (NULL for negative
 * caching) to the routing cache.
 */
static inline void
rtcache_add_v4(const ip4_addr_t * ipaddr, struct route_entry * route)
{

	rtcache_v4addr = *ipaddr;
	rtcache_v4route = route;
	rtcache_v4set = TRUE;
}

/*
 * Reset the IPv4 routing cache.
 */
static void
rtcache_reset_v4(void)
{

	rtcache_v4set = FALSE;
}

/*
 * Look up the given IPv6 address in the routing cache.  If there is a match,
 * return TRUE with the associated route in 'route', possibly NULL if a
 * negative result was cached.  Return FALSE if the routing cache does not
 * cache the given IPv6 address.
 */
static inline int
rtcache_lookup_v6(const ip6_addr_t * ipaddr, struct route_entry ** route)
{

	if (rtcache_v6set && ip6_addr_cmp(&rtcache_v6addr, ipaddr)) {
		*route = rtcache_v6route;

		return TRUE;
	} else
		return FALSE;
}

/*
 * Add the given IPv6 address and the given routing entry (NULL for negative
 * caching) to the routing cache.  Caching of scoped addresses without zones is
 * not supported.
 */
static inline void
rtcache_add_v6(const ip6_addr_t * ipaddr, struct route_entry * route)
{

	rtcache_v6addr = *ipaddr;
	rtcache_v6route = route;
	rtcache_v6set = TRUE;
}

/*
 * Reset the IPv6 routing cache.
 */
static void
rtcache_reset_v6(void)
{

	rtcache_v6set = FALSE;
}

/*
 * Initialize the routing module.
 */
void
route_init(void)
{
	unsigned int slot;

	/* Initialize the routing trees. */
	rttree_init(&route_tree[ROUTE_TREE_V4], IP4_BITS);
	rttree_init(&route_tree[ROUTE_TREE_V6], IP6_BITS);

	/* Initialize the list of free routing entries. */
	SIMPLEQ_INIT(&route_freelist);

	for (slot = 0; slot < __arraycount(route_array); slot++)
		SIMPLEQ_INSERT_TAIL(&route_freelist, &route_array[slot],
		    re_next);

	/* Reset the routing cache. */
	rtcache_init();
}

/*
 * Prepare for a routing tree operation by converting the given IPv4 address
 * into a raw address that can be used in that routing tree operation.
 */
static inline void
route_prepare_v4(const ip4_addr_t * ip4addr, uint8_t rtaddr[ROUTE_ADDR_MAX])
{
	uint32_t val;

	val = ip4_addr_get_u32(ip4addr);

	memcpy(rtaddr, &val, sizeof(val));
}

/*
 * Prepare for a routing tree operation by converting the given IPv6 address
 * into a raw address that can be used in that routing tree operation.  If the
 * given prefix length allows for it, also incorporate the address zone.
 */
static inline void
route_prepare_v6(const ip6_addr_t * ip6addr, unsigned int prefix,
	uint8_t rtaddr[ROUTE_ADDR_MAX])
{

	assert(sizeof(ip6addr->addr) == IP6_BITS / NBBY);

	/*
	 * TODO: in most cases, we could actually return a pointer to the
	 * address contained in the given lwIP IP address structure.  However,
	 * doing so would make a lot things quite a bit messier around here,
	 * but the small performance gain may still make it worth it.
	 */
	memcpy(rtaddr, ip6addr->addr, sizeof(ip6addr->addr));

	/*
	 * Embed the zone ID into the address, KAME style.  This is the
	 * easiest way to have link-local addresses for multiple interfaces
	 * coexist in a single routing tree.  Do this only if the full zone ID
	 * would be included in the prefix though, or we might de-normalize the
	 * address.
	 */
	if (ip6_addr_has_zone(ip6addr) && prefix >= 32)
		rtaddr[3] = ip6_addr_zone(ip6addr);
}

/*
 * Prepare for a routing tree operation by converting the given IP address into
 * a raw address that can be used in that routing tree operation.  The given
 * address's zone ID is embedded "KAME-style" into the raw (IPv6) address when
 * applicable and if the given prefix length allows for it.  Return the index
 * of the routing tree to use (ROUTE_TREE_V4 or ROUTE_TREE_V6).
 */
static unsigned int
route_prepare(const ip_addr_t * ipaddr, unsigned int prefix,
	uint8_t rtaddr[ROUTE_ADDR_MAX])
{

	switch (IP_GET_TYPE(ipaddr)) {
	case IPADDR_TYPE_V4:
		route_prepare_v4(ip_2_ip4(ipaddr), rtaddr);

		return ROUTE_TREE_V4;

	case IPADDR_TYPE_V6:
		route_prepare_v6(ip_2_ip6(ipaddr), prefix, rtaddr);

		return ROUTE_TREE_V6;

	default:
		panic("unknown IP address type: %u", IP_GET_TYPE(ipaddr));
	}
}

/*
 * The given routing tree (ROUTE_TREE_V4 or ROUTE_TREE_V6) has been updated.
 * Invalidate any cache entries that may now have become stale, both locally
 * and in lwIP.
 */
static void
route_updated(unsigned int tree)
{

	if (tree == ROUTE_TREE_V6) {
		rtcache_reset_v6();

		/*
		 * Also clear the lwIP ND6 destination cache, which may now
		 * contain entries for the wrong gateway.
		 */
		nd6_clear_destination_cache();
	} else
		rtcache_reset_v4();
}

/*
 * Add a route to the appropriate routing table.  The address, address zone,
 * prefix, and RTF_HOST flag in the flags field make up the identity of the
 * route.  If the flags field contains RTF_GATEWAY, a gateway must be given;
 * otherwise, it must be NULL.  The route is associated with the given
 * interface, which may not be NULL.  The caller must ensure that the flags
 * field does not contain unsupported flags.  On success, return OK, and also
 * also announce the addition.  On failure, return a negative error code.
 */
int
route_add(const ip_addr_t * addr, unsigned int prefix,
	const ip_addr_t * gateway, struct ifdev * ifdev, unsigned int flags,
	const struct rtsock_request * rtr)
{
	struct route_entry *route;
	unsigned int tree, byte;
	int r;

	assert(flags & RTF_UP);
	assert(!!(flags & RTF_GATEWAY) == (gateway != NULL));
	assert(ifdev != NULL);

	/* Get a routing entry, if any are available. */
	if (SIMPLEQ_EMPTY(&route_freelist))
		return ENOBUFS;

	route = SIMPLEQ_FIRST(&route_freelist);

	/*
	 * Perform sanity checks on the input, and fill in enough of the
	 * routing entry to be able to try and add it to the routing tree.
	 */
	memset(route->re_addr, 0, sizeof(route->re_addr));

	tree = route_prepare(addr, prefix, route->re_addr);

	switch (tree) {
	case ROUTE_TREE_V4:
		if (prefix > IP4_BITS ||
		    (prefix != IP4_BITS && (flags & RTF_HOST)))
			return EINVAL;

		flags &= ~RTF_IPV6;

		break;

	case ROUTE_TREE_V6:
		if (prefix > IP6_BITS ||
		    (prefix != IP6_BITS && (flags & RTF_HOST)))
			return EINVAL;

		flags |= RTF_IPV6;

		break;

	default:
		return EINVAL;
	}

	/* Generate the (raw) network mask.  This is protocol agnostic! */
	addr_make_netmask(route->re_mask, sizeof(route->re_mask), prefix);

	/* The given address must be normalized to its mask. */
	for (byte = 0; byte < __arraycount(route->re_addr); byte++)
		if ((route->re_addr[byte] & ~route->re_mask[byte]) != 0)
			return EINVAL;

	/*
	 * Attempt to add the routing entry.  Host-type entries do not have an
	 * associated mask, enabling ever-so-slightly faster matching.
	 */
	if ((r = rttree_add(&route_tree[tree], &route->re_entry,
	    route->re_addr, (flags & RTF_HOST) ? NULL : route->re_mask,
	    prefix)) != OK)
		return r;

	/*
	 * Success.  Finish the routing entry.  Remove the entry from the free
	 * list before assigning re_ifdev, as these two use the same memory.
	 */
	SIMPLEQ_REMOVE_HEAD(&route_freelist, re_next);

	route->re_ifdev = ifdev;
	route->re_flags = flags;

	/*
	 * Store the gateway if one is given.  Store the address in lwIP format
	 * because that is the easiest way use it later again.  Store it as a
	 * union to keep the route entry structure as small as possible.  Store
	 * the address without its zone, because the gateway's address zone is
	 * implied by its associated ifdev.
	 *
	 * If no gateway is given, this is a link-type route, i.e., a route for
	 * a local network, with all nodes directly connected and reachable.
	 */
	if (flags & RTF_GATEWAY) {
		if (flags & RTF_IPV6)
			ip6_addr_copy_to_packed(route->re_gw6,
			    *ip_2_ip6(gateway));
		else
			ip4_addr_copy(route->re_gw4, *ip_2_ip4(gateway));
	}

	/* We have made routing changes. */
	route_updated(tree);

	/* Announce the route addition. */
	rtsock_msg_route(route, RTM_ADD, rtr);

	return OK;
}

/*
 * Check whether it is possible to add a route for the given destination to the
 * corresponding routing table, that is, a subsequent route_add() call for this
 * destination address is guaranteed to succeed (if all its parameters are
 * valid).  Return TRUE if adding the route is guaranteed to succeed, or FALSE
 * if creating a route for the given destination would fail.
 */
int
route_can_add(const ip_addr_t * addr, unsigned int prefix,
	int is_host __unused)
{
	uint8_t rtaddr[ROUTE_ADDR_MAX];
	unsigned int tree;

	tree = route_prepare(addr, prefix, rtaddr);

	/*
	 * The corresponding routing tree must not already contain an exact
	 * match for the destination.  If the routing tree implementation is
	 * ever extended with support for coexisting host and net entries with
	 * the same prefix, we should also pass in 'is_host' here.
	 */
	if (rttree_lookup_exact(&route_tree[tree], rtaddr, prefix) != NULL)
		return FALSE;

	/* There must be a routing entry on the free list as well. */
	return !SIMPLEQ_EMPTY(&route_freelist);
}

/*
 * Find a route with the exact given route identity.  Return the route if
 * found, or NULL if no route exists with this identity.
 */
struct route_entry *
route_find(const ip_addr_t * addr, unsigned int prefix, int is_host)
{
	struct rttree_entry *entry;
	struct route_entry *route;
	uint8_t rtaddr[ROUTE_ADDR_MAX];
	unsigned int tree;

	tree = route_prepare(addr, prefix, rtaddr);

	entry = rttree_lookup_exact(&route_tree[tree], rtaddr, prefix);
	if (entry == NULL)
		return NULL;

	route = (struct route_entry *)entry;

	/*
	 * As long as the routing tree code does not support coexisting host
	 * and net entries with the same prefix, we have to check the type.
	 */
	if (!!(route->re_flags & RTF_HOST) != is_host)
		return NULL;

	return route;
}

/*
 * A route lookup failed for the given IP address.  Generate an RTM_MISS
 * message on routing sockets.
 */
static void
route_miss(const ip_addr_t * ipaddr)
{
	union sockaddr_any addr;
	socklen_t addr_len;

	addr_len = sizeof(addr);

	addr_put_inet(&addr.sa, &addr_len, ipaddr, TRUE /*kame*/, 0 /*port*/);

	rtsock_msg_miss(&addr.sa);
}

/*
 * A route lookup failed for the given IPv4 address.  Generate an RTM_MISS
 * message on routing sockets.
 */
static void
route_miss_v4(const ip4_addr_t * ip4addr)
{
	ip_addr_t ipaddr;

	ip_addr_copy_from_ip4(ipaddr, *ip4addr);

	route_miss(&ipaddr);
}

/*
 * A route lookup failed for the given IPv6 address.  Generate an RTM_MISS
 * message on routing sockets.
 */
static void
route_miss_v6(const ip6_addr_t * ip6addr)
{
	ip_addr_t ipaddr;

	ip_addr_copy_from_ip6(ipaddr, *ip6addr);

	route_miss(&ipaddr);
}

/*
 * Look up the most narrow matching routing entry for the given IPv4 address.
 * Return the routing entry if one exists at all, or NULL otherwise.  This
 * function performs caching.
 */
static inline struct route_entry *
route_lookup_v4(const ip4_addr_t * ip4addr)
{
	uint8_t rtaddr[ROUTE_ADDR_MAX];
	struct route_entry *route;

	/*
	 * Look up the route for the destination IP address, unless we have a
	 * cached route entry.  We cache negatives in order to avoid generating
	 * lots of RTM_MISS messages for the same destination in a row.
	 */
	if (rtcache_lookup_v4(ip4addr, &route))
		return route;

	route_prepare_v4(ip4addr, rtaddr);

	route = (struct route_entry *)
	    rttree_lookup_match(&route_tree[ROUTE_TREE_V4], rtaddr);

	/* Cache the result, even if we found no route. */
	rtcache_add_v4(ip4addr, route);

	return route;
}

/*
 * Look up the most narrow matching routing entry for the given IPv6 address,
 * taking into account its zone ID if applicable.  Return the routing entry if
 * one exists at all, or NULL otherwise.  This function performs caching.
 */
static inline struct route_entry *
route_lookup_v6(const ip6_addr_t * ip6addr)
{
	uint8_t rtaddr[ROUTE_ADDR_MAX];
	struct route_entry *route;
	int use_cache;

	/*
	 * We do not support caching of addresses that should have a zone but
	 * do not: in different contexts, such addresses could yield different
	 * routes.
	 */
	use_cache = !ip6_addr_lacks_zone(ip6addr, IP6_UNKNOWN);

	if (use_cache && rtcache_lookup_v6(ip6addr, &route))
		return route;

	route_prepare_v6(ip6addr, IP6_BITS, rtaddr);

	route = (struct route_entry *)
	    rttree_lookup_match(&route_tree[ROUTE_TREE_V6], rtaddr);

	/* Cache the result, even if no route was found. */
	if (use_cache)
		rtcache_add_v6(ip6addr, route);

	return route;
}

/*
 * Look up the most narrow matching routing entry for the given IP address,
 * taking into account its zone ID if applicable.  Return the routing entry if
 * one exists at all, or NULL otherwise.  This function performs caching.
 */
struct route_entry *
route_lookup(const ip_addr_t * addr)
{

	if (IP_IS_V4(addr))
		return route_lookup_v4(ip_2_ip4(addr));
	else
		return route_lookup_v6(ip_2_ip6(addr));
}

/*
 * Change an existing routing entry.  Its flags are always updated to the new
 * set of given flags, although certain flags are always preserved.  If the
 * new flags set has RTF_GATEWAY set and 'gateway' is not NULL, update the
 * gateway associated with the route.  If 'ifdev' is not NULL, reassociate the
 * route with the given interface; this will not affect the zone of the
 * route's destination address.  On success, return OK, and also announce the
 * change.  On failure, return a negative error code.
 */
static int
route_change(struct route_entry * route, const ip_addr_t * gateway,
	struct ifdev * ifdev, unsigned int flags,
	const struct rtsock_request * rtr)
{
	unsigned int tree, preserve;

	tree = (route->re_flags & RTF_IPV6) ? ROUTE_TREE_V6 : ROUTE_TREE_V4;

	/* Update the associated interface (only) if a new one is given. */
	if (ifdev != NULL)
		route->re_ifdev = ifdev;

	/*
	 * These flags may not be changed.  RTF_UP should always be set anyway.
	 * RTF_HOST and RTF_IPV6 are part of the route's identity.  RTF_LOCAL
	 * should be preserved as well, although we will not get here if either
	 * the old or the new flags have it set anyway.
	 */
	preserve = RTF_UP | RTF_HOST | RTF_IPV6 | RTF_LOCAL;

	/* Always update the flags.  There is no way not to. */
	route->re_flags = (route->re_flags & preserve) | (flags & ~preserve);

	/*
	 * If a new gateway is given *and* RTF_GATEWAY is set, update the
	 * gateway.  If RTF_GATEWAY is not set, this is a link-type route with
	 * no gateway.  If no new gateway is given, we keep the gateway as is.
	 */
	if (gateway != NULL && (flags & RTF_GATEWAY)) {
		if (flags & RTF_IPV6)
			ip6_addr_copy_to_packed(route->re_gw6,
			    *ip_2_ip6(gateway));
		else
			ip4_addr_copy(route->re_gw4, *ip_2_ip4(gateway));
	}

	/* We have made routing changes. */
	route_updated(tree);

	/* Announce the route change. */
	rtsock_msg_route(route, RTM_CHANGE, rtr);

	return OK;
}

/*
 * Delete the given route, and announce its deletion.
 */
void
route_delete(struct route_entry * route, const struct rtsock_request * rtr)
{
	unsigned int tree;

	/* First announce the deletion, while the route is still around. */
	tree = (route->re_flags & RTF_IPV6) ? ROUTE_TREE_V6 : ROUTE_TREE_V4;

	rtsock_msg_route(route, RTM_DELETE, rtr);

	/* Then actually delete the route. */
	rttree_delete(&route_tree[tree], &route->re_entry);

	SIMPLEQ_INSERT_HEAD(&route_freelist, route, re_next);

	/* We have made routing changes. */
	route_updated(tree);
}

/*
 * Delete all routes associated with the given interface, typically as part of
 * destroying the interface.
 */
void
route_clear(struct ifdev * ifdev)
{
	struct rttree_entry *entry, *parent;
	struct route_entry *route;
	unsigned int tree;

	/*
	 * Delete all routes associated with the given interface.  Fortunately,
	 * we need not also delete addresses zoned to the given interface,
	 * because no route can be created with a zone ID that does not match
	 * the associated interface.  That is the main reason why we ignore
	 * zone IDs for gateways when adding or changing routes..
	 */
	for (tree = 0; tree < NR_ROUTE_TREE; tree++) {
		parent = NULL;

		while ((entry = rttree_enum(&route_tree[tree],
		    parent)) != NULL) {
			route = (struct route_entry *)entry;

			if (route->re_ifdev == ifdev)
				route_delete(route, NULL /*request*/);
			else
				parent = entry;
		}
	}
}

/*
 * Process a routing command specifically for an IPv4 or IPv6 route, as one of
 * the specific continuations of processing started by route_process().  The
 * RTM_ routing command is given as 'type'.  The route destination is given as
 * 'dst_addr'; its address type determines whether the operation is for IPv4 or
 * IPv6.  The sockaddr structures for 'mask' and 'gateway' are passed on as is
 * and may have to be parsed here if not NULL.  'ifdev' is the interface to be
 * associated with a route; it is non-NULL only if an interface name (IFP) or
 * address (IFA) was given.  The RTF_ flags field 'flags' has been checked
 * against the globally supported flags, but may have to be checked for flags
 * that do not apply to IPv4/IPv6 routes.  Return OK or a negative error code,
 * following the same semantics as route_process().
 */
static int
route_process_inet(unsigned int type, const ip_addr_t * dst_addr,
	const struct sockaddr * mask, const struct sockaddr * gateway,
	struct ifdev * ifdev, unsigned int flags,
	const struct rtsock_request * rtr)
{
	struct route_entry *route;
	ip_addr_t gw_storage, *gw_addr;
	struct ifdev *ifdev2;
	uint32_t zone;
	unsigned int prefix;
	int r;

	assert(!(flags & RTF_LLDATA));

	if ((flags & (RTF_DYNAMIC | RTF_MODIFIED | RTF_DONE | RTF_XRESOLVE |
	    RTF_LLINFO | RTF_CLONED | RTF_SRC | RTF_ANNOUNCE |
	    RTF_BROADCAST)) != 0)
		return EINVAL;

	/*
	 * For network entries, a network mask must be provided in all cases.
	 * For host entries, the network mask is ignored, and we use a prefix
	 * with all bits set.
	 */
	if (!(flags & RTF_HOST)) {
		if (mask == NULL)
			return EINVAL;

		if ((r = addr_get_netmask(mask, mask->sa_len,
		    IP_GET_TYPE(dst_addr), &prefix, NULL /*ipaddr*/)) != OK)
			return r;
	} else {
		if (IP_IS_V4(dst_addr))
			prefix = IP4_BITS;
		else
			prefix = IP6_BITS;
	}

	gw_addr = NULL;

	/*
	 * Determine the gateway and interface for the routing entry, if
	 * applicable.
	 */
	if (type == RTM_ADD || type == RTM_CHANGE) {
		/*
		 * The RTF_UP flag must always be set, but only if the flags
		 * field is used at all.
		 */
		if (!(flags & RTF_UP))
			return EINVAL;

		if ((flags & RTF_GATEWAY) && gateway != NULL) {
			if ((r = addr_get_inet(gateway, gateway->sa_len,
			    IP_GET_TYPE(dst_addr), &gw_storage, TRUE /*kame*/,
			    NULL /*port*/)) != OK)
				return r;

			gw_addr = &gw_storage;

			/*
			 * We use the zone of the gateway to help determine the
			 * interface, but we do not reject a mismatching zone
			 * here.  The reason for this is that we do not want
			 * routes that have zones for an interface other than
			 * the one associated with the route, as that could
			 * create a world of trouble: packets leaving their
			 * zone, complications with cleaning up interfaces..
			 */
			if (IP_IS_V6(gw_addr) &&
			    ip6_addr_has_zone(ip_2_ip6(gw_addr))) {
				zone = ip6_addr_zone(ip_2_ip6(gw_addr));

				ifdev2 = ifdev_get_by_index(zone);

				if (ifdev != NULL && ifdev != ifdev2)
					return EINVAL;
				else
					ifdev = ifdev2;
			}

			/*
			 * If we still have no interface at this point, see if
			 * we can find one based on just the gateway address.
			 * See if a locally attached network owns the address.
			 * That may not succeed, leaving ifdev set to NULL.
			 */
			if (ifdev == NULL)
				ifdev = ifaddr_map_by_subnet(gw_addr);
		}

		/*
		 * When adding routes, all necessary information must be given.
		 * When changing routes, we can leave some settings as is.
		 */
		if (type == RTM_ADD) {
			if ((flags & RTF_GATEWAY) && gw_addr == NULL)
				return EINVAL;

			/* TODO: try harder to find a matching interface.. */
			if (ifdev == NULL)
				return ENETUNREACH;
		}
	}

	/*
	 * All route commands except RTM_ADD require that a route exists for
	 * the given identity, although RTM_GET, when requesting a host entry,
	 * may return a wider (network) route based on just the destination
	 * address.
	 */
	if (type != RTM_ADD) {
		/* For RTM_GET (only), a host query may return a net route. */
		if (type == RTM_GET && (flags & RTF_HOST))
			route = route_lookup(dst_addr);
		else
			route = route_find(dst_addr, prefix,
			    !!(flags & RTF_HOST));

		if (route == NULL)
			return ESRCH;
	} else
		route = NULL;

	/* Process the actual routing command. */
	switch (type) {
	case RTM_ADD:
		return route_add(dst_addr, prefix, gw_addr, ifdev, flags, rtr);

	case RTM_CHANGE:
		/* Routes for local addresses are immutable. */
		if (route_is_immutable(route))
			return EPERM;

		return route_change(route, gw_addr, ifdev, flags, rtr);

	case RTM_DELETE:
		/* Routes for local addresses are immutable. */
		if (route_is_immutable(route))
			return EPERM;

		route_delete(route, rtr);

		return OK;

	case RTM_LOCK:
		/*
		 * TODO: implement even the suggestion that we support this.
		 * For now, we do not keep per-route metrics, let alone change
		 * them dynamically ourselves, so "locking" metrics is really
		 * not a concept that applies to us.  We may however have to
		 * save the lock mask and return it in queries..
		 */
		/* FALLTHROUGH */
	case RTM_GET:
		/* Simply generate a message for the route we just found. */
		rtsock_msg_route(route, type, rtr);

		return OK;

	default:
		return EINVAL;
	}
}

/*
 * Process a routing command from a routing socket.  The RTM_ type of command
 * is given as 'type', and is one of RTM_ADD, RTM_CHANGE, RTM_DELETE, RTM_GET,
 * RTM_LOCK.  In addition, the function takes a set of sockaddr pointers as
 * provided by the routing command.  Each of these sockaddr pointers may be
 * NULL; if not NULL, the structure is at least large enough to contain the
 * address length (sa_len) and family (sa_family), and the length never exceeds
 * the amount of memory used to store the sockaddr structure.  However, the
 * length itself has not yet been checked against the expected protocol
 * structure and could even be zero.  The command's RTF_ routing flags and
 * metrics are provided as well.  On success, return OK, in which case the
 * caller assumes that a routing socket announcement for the processed command
 * has been sent already (passing on 'rtr' to the announcement function as is).
 * On failure, return a negative error code; in that case, the caller will send
 * a failure response on the original routing socket itself.
 */
int
route_process(unsigned int type, const struct sockaddr * dst,
	const struct sockaddr * mask, const struct sockaddr * gateway,
	const struct sockaddr * ifp, const struct sockaddr * ifa,
	unsigned int flags, unsigned long inits,
	const struct rt_metrics * rmx, const struct rtsock_request * rtr)
{
	struct ifdev *ifdev, *ifdev2;
	char name[IFNAMSIZ];
	ip_addr_t dst_addr, if_addr;
	uint32_t zone;
	uint8_t addr_type;
	int r;

	/*
	 * The identity of a route is determined by its destination address,
	 * destination zone, prefix length, and whether it is a host entry
	 * or not.  If it is a host entry (RTF_HOST is set), the prefix length
	 * is implied by the protocol; otherwise it should be obtained from the
	 * given netmask if necessary.  For link-local addresses, the zone ID
	 * must be embedded KAME-style in the destination address.  A
	 * destination address must always be given.  The destination address
	 * also determines the overall address family.
	 */
	if (dst == NULL)
		return EINVAL;

	switch (dst->sa_family) {
	case AF_INET:
		addr_type = IPADDR_TYPE_V4;
		break;
#ifdef INET6
	case AF_INET6:
		addr_type = IPADDR_TYPE_V6;
		break;
#endif /* INET6 */
	default:
		return EAFNOSUPPORT;
	}

	if ((r = addr_get_inet(dst, dst->sa_len, addr_type, &dst_addr,
	    TRUE /*kame*/, NULL /*port*/)) != OK)
		return r;

	/*
	 * Perform a generic test on the given flags.  This covers everything
	 * we support at all, plus a few flags we ignore.  Specific route types
	 * may have further restrictions; those tests are performed later.
	 */
	if ((flags & ~(RTF_UP | RTF_GATEWAY | RTF_HOST | RTF_REJECT |
	    RTF_CLONING | RTF_LLINFO | RTF_LLDATA | RTF_STATIC |
	    RTF_BLACKHOLE | RTF_CLONED | RTF_PROTO2 | RTF_PROTO1)) != 0)
		return EINVAL;

	ifdev = NULL;

	if (type == RTM_ADD || type == RTM_CHANGE) {
		/*
		 * If an interface address or name is given, use that to
		 * identify the target interface.  If both are given, make sure
		 * that both identify the same interface--a hopefully helpful
		 * feature to detect wrong route(8) usage (NetBSD simply takes
		 * IFP over IFA).  An empty interface name is ignored on the
		 * basis that libc link_addr(3) is broken.
		 */
		if (ifp != NULL) {
			if ((r = addr_get_link(ifp, ifp->sa_len, name,
			    sizeof(name), NULL /*hwaddr*/,
			    0 /*hwaddr_len*/)) != OK)
				return r;

			if (name[0] != '\0' &&
			    (ifdev = ifdev_find_by_name(name)) == NULL)
				return ENXIO;
		}

		if (ifa != NULL) {
			/*
			 * This is similar to retrieval of source addresses in
			 * ipsock, with the difference that we do not impose
			 * that a zone ID be given for link-local addresses.
			 */
			if ((r = addr_get_inet(ifa, ifa->sa_len, addr_type,
			    &if_addr, TRUE /*kame*/, NULL /*port*/)) != OK)
				return r;

			if ((ifdev2 = ifaddr_map_by_addr(&if_addr)) == NULL)
				return EADDRNOTAVAIL;

			if (ifdev != NULL && ifdev != ifdev2)
				return EINVAL;
			else
				ifdev = ifdev2;
		}

		/*
		 * If the destination address has a zone, then it must not
		 * conflict with the interface, if one was given.  If not, we
		 * may use it to decide the interface to use for the route.
		 */
		if (IP_IS_V6(&dst_addr) &&
		    ip6_addr_has_zone(ip_2_ip6(&dst_addr))) {
			if (ifdev == NULL) {
				zone = ip6_addr_zone(ip_2_ip6(&dst_addr));

				ifdev = ifdev_get_by_index(zone);
			} else {
				if (!ip6_addr_test_zone(ip_2_ip6(&dst_addr),
				    ifdev_get_netif(ifdev)))
					return EADDRNOTAVAIL;
			}
		}
	}

	/*
	 * For now, no initializers are supported by any of the sub-processing
	 * routines, so outright reject requests that set any initializers.
	 * Most importantly, we do not support per-route MTU settings (RTV_MTU)
	 * because lwIP would not use them, and we do not support non-zero
	 * expiry (RTV_EXPIRE) because for IPv4/IPv6 routes it is not a widely
	 * used feature and for ARP/NDP we would have to change lwIP.
	 * dhcpcd(8) does supply RTV_MTU, we have to ignore that option rather
	 * than reject it, unfortunately.  arp(8) always sets RTV_EXPIRE, so we
	 * reject only non-zero expiry there.
	 */
	if ((inits & ~(RTV_EXPIRE | RTV_MTU)) != 0 ||
	    ((inits & RTV_EXPIRE) != 0 && rmx->rmx_expire != 0))
		return ENOSYS;

	/*
	 * From here on, the processing differs for ARP, NDP, and IP routes.
	 * As of writing, our userland is from NetBSD 7, which puts link-local
	 * route entries in its main route tables.  This means we would have to
	 * search for existing routes before we can determine whether, say, a
	 * RTM_GET request is for an IP or an ARP route entry.  As of NetBSD 8,
	 * the link-local administration is separated, and all requests use the
	 * RTF_LLDATA flag to indicate that they are for ARP/NDP routes rather
	 * than IP routes.  Since that change makes things much cleaner for us,
	 * we borrow from the future, patching arp(8) and ndp(8) to add the
	 * RTF_LLDATA flag now, so that we can implement a clean split here.
	 */
	if (!(flags & RTF_LLDATA))
		return route_process_inet(type, &dst_addr, mask, gateway,
		    ifdev, flags, rtr);
	else
		return lldata_process(type, &dst_addr, gateway, ifdev, flags,
		    rtr);
}

/*
 * Return the routing flags (RTF_) for the given routing entry.  Strip out any
 * internal flags.
 */
unsigned int
route_get_flags(const struct route_entry * route)
{

	return route->re_flags & ~RTF_IPV6;
}

/*
 * Return TRUE if the given routing entry is for the IPv6 address family, or
 * FALSE if it is for IPv4.
 */
int
route_is_ipv6(const struct route_entry * route)
{

	return !!(route->re_flags & RTF_IPV6);
}

/*
 * Return the interface associated with the given routing entry.  The resulting
 * interface is never NULL.
 */
struct ifdev *
route_get_ifdev(const struct route_entry * route)
{

	return route->re_ifdev;
}

/*
 * Convert the given raw routing address pointed to by 'rtaddr' into a
 * lwIP-style IP address 'ipaddr' of type 'type', which must by IPADDR_TYPE_V4
 * or IPADDR_TYPE_V6.
 */
static void
route_get_addr(ip_addr_t * ipaddr, const uint8_t * rtaddr, uint8_t type)
{
	ip6_addr_t *ip6addr;
	uint32_t val, zone;

	/*
	 * Convert the routing address to a lwIP-type IP address.  Take out the
	 * KAME-style embedded zone, if needed.
	 */
	memset(ipaddr, 0, sizeof(*ipaddr));
	IP_SET_TYPE(ipaddr, type);

	switch (type) {
	case IPADDR_TYPE_V4:
		memcpy(&val, rtaddr, sizeof(val));

		ip_addr_set_ip4_u32(ipaddr, val);

		break;

	case IPADDR_TYPE_V6:
		ip6addr = ip_2_ip6(ipaddr);

		memcpy(ip6addr->addr, rtaddr, sizeof(ip6addr->addr));

		if (ip6_addr_has_scope(ip6addr, IP6_UNKNOWN)) {
			zone = ntohl(ip6addr->addr[0]) & 0x0000ffffU;

			ip6addr->addr[0] &= PP_HTONL(0xffff0000U);

			ip6_addr_set_zone(ip6addr, zone);
		}

		break;

	default:
		panic("unknown IP address type: %u", type);
	}
}

/*
 * Obtain information about an IPv4 or IPv6 routing entry, by filling 'addr',
 * 'mask', 'gateway', and optionally (if not NULL) 'ifp' and 'ifa' with
 * sockaddr-type data for each of those fields.  Also store the associated
 * interface in 'ifdevp', the routing entry's flags in 'flags', and the route's
 * usage count in 'use'.
 */
void
route_get(const struct route_entry * route, union sockaddr_any * addr,
	union sockaddr_any * mask, union sockaddr_any * gateway,
	union sockaddr_any * ifp, union sockaddr_any * ifa,
	struct ifdev ** ifdevp, unsigned int * flags, unsigned int * use)
{
	const ip_addr_t *src_addr;
	ip_addr_t dst_addr, gw_addr;
	struct ifdev *ifdev;
	socklen_t addr_len;
	uint8_t type;

	type = (route->re_flags & RTF_IPV6) ? IPADDR_TYPE_V6 : IPADDR_TYPE_V4;

	/* Get the destination address. */
	route_get_addr(&dst_addr, route->re_addr, type);

	addr_len = sizeof(*addr);

	addr_put_inet(&addr->sa, &addr_len, &dst_addr, TRUE /*kame*/,
	    0 /*port*/);

	/* Get the network mask, if applicable. */
	if (!(route->re_flags & RTF_HOST)) {
		addr_len = sizeof(*mask);

		addr_put_netmask(&mask->sa, &addr_len, type,
		    rttree_get_prefix(&route->re_entry));
	} else
		mask->sa.sa_len = 0;

	/* Get the gateway, which may be an IP address or a local link. */
	addr_len = sizeof(*gateway);

	ifdev = route->re_ifdev;

	if (route->re_flags & RTF_GATEWAY) {
		if (type == IPADDR_TYPE_V4)
			ip_addr_copy_from_ip4(gw_addr, route->re_gw4);
		else
			ip_addr_copy_from_ip6_packed(gw_addr, route->re_gw6);

		addr_put_inet(&gateway->sa, &addr_len, &gw_addr, TRUE /*kame*/,
		    0 /*port*/);
	} else {
		addr_put_link(&gateway->sa, &addr_len, ifdev_get_index(ifdev),
		    ifdev_get_iftype(ifdev), NULL /*name*/, NULL /*hwaddr*/,
		    0 /*hwaddr_len*/);
	}

	/* Get the associated interface name. */
	if (ifp != NULL) {
		addr_len = sizeof(*ifp);

		addr_put_link(&ifp->sa, &addr_len, ifdev_get_index(ifdev),
		    ifdev_get_iftype(ifdev), ifdev_get_name(ifdev),
		    NULL /*hwaddr*/, 0 /*hwaddr_len*/);
	}

	/* Get the associated source address, if we can determine one. */
	if (ifa != NULL) {
		src_addr = ifaddr_select(&dst_addr, ifdev, NULL /*ifdevp*/);

		if (src_addr != NULL) {
			addr_len = sizeof(*ifa);

			addr_put_inet(&ifa->sa, &addr_len, src_addr,
			    TRUE /*kame*/, 0 /*port*/);
		} else
			ifa->sa.sa_len = 0;
	}

	/* Get other fields. */
	*flags = route_get_flags(route);	/* strip any internal flags */
	*ifdevp = ifdev;
	*use = route->re_use;
}

/*
 * Enumerate IPv4 routing entries.  Return the first IPv4 routing entry if
 * 'last' is NULL, or the next routing entry after 'last' if it is not NULL.
 * In both cases, the return value may be NULL if there are no more routes.
 */
struct route_entry *
route_enum_v4(struct route_entry * last)
{

	assert(last == NULL || !(last->re_flags & RTF_IPV6));

	return (struct route_entry *)rttree_enum(&route_tree[ROUTE_TREE_V4],
	    (last != NULL) ? &last->re_entry : NULL);
}

/*
 * Enumerate IPv6 routing entries.  Return the first IPv6 routing entry if
 * 'last' is NULL, or the next routing entry after 'last' if it is not NULL.
 * In both cases, the return value may be NULL if there are no more routes.
 */
struct route_entry *
route_enum_v6(struct route_entry * last)
{

	assert(last == NULL || (last->re_flags & RTF_IPV6));

	return (struct route_entry *)rttree_enum(&route_tree[ROUTE_TREE_V6],
	    (last != NULL) ? &last->re_entry : NULL);
}

/*
 * lwIP IPv4 routing function.   Given an IPv4 destination address, look up and
 * return the target interface, or NULL if there is no route to the address.
 *
 * This is a full replacement of the corresponding lwIP function, which should
 * be overridden with weak symbols, using patches against the lwIP source code.
 * As such, the lwIP headers should already provide the correct prototype for
 * this function.  If not, something will have changed in the lwIP
 * implementation, and this code must be revised accordingly.
 */
struct netif *
ip4_route(const ip4_addr_t * dst)
{
	struct route_entry *route;
	struct ifdev *ifdev;

	/*
	 * Look up the route for the destination IPv4 address.  If no route is
	 * found at all, return NULL to the caller.
	 */
	if ((route = route_lookup_v4(dst)) == NULL) {
		route_miss_v4(dst);

		return NULL;
	}

	/*
	 * For now, we increase the use counter only for actual route lookups,
	 * and not for gateway lookups or user queries.  As of writing,
	 * route(8) does not print this number anyway..
	 */
	route->re_use++;

	/*
	 * For all packets that are supposed to be rejected or blackholed, use
	 * a loopback interface, regardless of the interface to which the route
	 * is associated (even though it will typically be lo0 anyway).  The
	 * reason for this is that on packet output, we perform another route
	 * route lookup just to check for rejection/blackholing, but for
	 * efficiency reasons, we limit such checks to loopback interfaces:
	 * loopback traffic will typically use only one IP address anyway, thus
	 * limiting route misses from such rejection/blackhole route lookups as
	 * much as we can.  The lookup is implemented in route_output_v4().  We
	 * divert only if the target interface is not a loopback interface
	 * already, mainly to allow userland tests to create blackhole routes
	 * to a specific loopback interface for testing purposes.
	 *
	 * It is not correct to return NULL for RTF_REJECT routes here, because
	 * this could cause e.g. connect() calls to fail immediately, which is
	 * not how rejection should work.  Related: a previous incarnation of
	 * support for these flags used a dedicated netif to eliminate the
	 * extra route lookup on regular output altogether, but in the current
	 * situation, that netif would have to be assigned (IPv4 and IPv6)
	 * addresses in order not to break e.g. connect() in the same way.
	 */
	if ((route->re_flags & (RTF_REJECT | RTF_BLACKHOLE)) &&
	    !ifdev_is_loopback(route->re_ifdev))
		ifdev = ifdev_get_loopback();
	else
		ifdev = route->re_ifdev;

	return ifdev_get_netif(ifdev);
}

/*
 * lwIP IPv4 routing hook.  Since this hook is called only from lwIP's own
 * ip4_route() implementation, this hook must never fire.  If it does, either
 * something is wrong with overriding ip4_route(), or lwIP added other places
 * from which this hook is called.  Both cases are highly problematic and must
 * be resolved somehow, which is why we simply call panic() here.
 */
struct netif *
lwip_hook_ip4_route(const ip4_addr_t * dst)
{

	panic("IPv4 routing hook called - this should not happen!");
}

/*
 * lwIP IPv4 ARP gateway hook.
 */
const ip4_addr_t *
lwip_hook_etharp_get_gw(struct netif * netif, const ip4_addr_t * ip4addr)
{
	static ip4_addr_t gw_addr; /* may be returned to the caller */
	struct route_entry *route;

	/* Look up the route for the destination IP address. */
	if ((route = route_lookup_v4(ip4addr)) == NULL)
		return NULL;

	/*
	 * This case could only ever trigger as a result of lwIP taking its own
	 * routing decisions instead of calling the IPv4 routing hook.  While
	 * not impossible, such cases should be extremely rare.  We cannot
	 * provide a meaningful gateway address in this case either, though.
	 */
	if (route->re_ifdev != netif_get_ifdev(netif)) {
		printf("LWIP: unexpected interface for gateway lookup\n");

		return NULL;
	}

	/*
	 * If this route has a gateway, return the IP address of the gateway.
	 * Otherwise, the route is for a local network, and we would typically
	 * not get here because lwIP performs the local-network check itself.
	 * It is possible that the local network consists of more than one IP
	 * range, and the user has configured a route for the other range.  In
	 * that case, return the IP address of the actual destination.
	 *
	 * We store a packed version of the IPv4 address, so reconstruct the
	 * unpacked version to a static variable first - for consistency with
	 * the IPv6 code.
	 */
	if (route->re_flags & RTF_GATEWAY) {
		ip4_addr_copy(gw_addr, route->re_gw4);

		return &gw_addr;
	} else
		return ip4addr;
}

/*
 * lwIP IPv6 routing function.   Given an IPv6 source and destination address,
 * look up and return the target interface, or NULL if there is no route to the
 * address.  Our routing algorithm is destination-based, meaning that the
 * source address must be considered only to resolve zone ambiguity.
 *
 * This is a full replacement of the corresponding lwIP function, which should
 * be overridden with weak symbols, using patches against the lwIP source code.
 * As such, the lwIP headers should already provide the correct prototype for
 * this function.  If not, something will have changed in the lwIP
 * implementation, and this code must be revised accordingly.
 */
struct netif *
ip6_route(const ip6_addr_t * src, const ip6_addr_t * dst)
{
	struct route_entry *route;
	struct ifdev *ifdev;
	ip6_addr_t dst_addr;
	uint32_t zone;

	assert(src != NULL);
	assert(dst != NULL);

	/*
	 * If the destination address is scoped but has no zone, use the source
	 * address to determine a zone, which we then set on the destination
	 * address to find the route, if successful.  Obviously, the interface
	 * is not going to be different from the zone, but we do need to check
	 * other aspects of the route (e.g., one might want to null-route all
	 * multicast traffic).  In the case that no source address is given at
	 * all, first see if the destination address happens to be a locally
	 * assigned address.  In theory this could yield multiple matches, so
	 * pick the first one.  If not even that helps, we have absolutely
	 * nothing we can use to refine route selection.  We could pick an
	 * arbitrary interface in that case, but we currently don't.
	 */
	zone = IP6_NO_ZONE;

	if (ip6_addr_lacks_zone(dst, IP6_UNKNOWN)) {
		if (ip6_addr_has_zone(src))
			zone = ip6_addr_zone(src);
		else if (!ip6_addr_isany(src)) {
			if ((ifdev = ifaddr_v6_map_by_addr(src)) == NULL)
				return NULL; /* should never happen */
			zone = ifdev_get_index(ifdev);
		} else {
			if ((ifdev = ifaddr_v6_map_by_addr(dst)) != NULL)
				zone = ifdev_get_index(ifdev);
			else
				return NULL; /* TODO: try harder */
		}

		if (zone != IP6_NO_ZONE) {
			dst_addr = *dst;

			ip6_addr_set_zone(&dst_addr, zone);

			dst = &dst_addr;
		}
	}

	route = route_lookup_v6(dst);

	/*
	 * Look up the route for the destination IPv6 address.  If no route is
	 * found at all, return NULL to the caller.
	 */
	if (route == NULL) {
		/*
		 * Since we rely on userland to create routes for on-link
		 * prefixes and default routers, we do not have to call lwIP's
		 * nd6_find_route() here.
		 */

		/* Generate an RTM_MISS message. */
		route_miss_v6(dst);

		return NULL;
	}

	/*
	 * We have found a route based on the destination address.  If we did
	 * not pick the destination address zone based on the source address,
	 * we should now check for source address zone violations.  Note that
	 * if even the destination address zone violates its target interface,
	 * this case will be caught by route_lookup_v6().
	 */
	if (zone == IP6_NO_ZONE &&
	    ifaddr_is_zone_mismatch(src, route->re_ifdev))
		return NULL;

	route->re_use++;

	/*
	 * See ip4_route() for an explanation of the use of loopback here.  For
	 * the IPv6 case, the matching logic is in route_output_v6().
	 */
	if ((route->re_flags & (RTF_REJECT | RTF_BLACKHOLE)) &&
	    !ifdev_is_loopback(route->re_ifdev))
		ifdev = ifdev_get_loopback();
	else
		ifdev = route->re_ifdev;

	/*
	 * If the selected interface would cause the destination address to
	 * leave its zone, fail route selection altogether.  This case may
	 * trigger especially for reject routes, for which the interface change
	 * to loopback may introduce a zone violation.
	 */
	if (ip6_addr_has_zone(dst) &&
	    !ip6_addr_test_zone(dst, ifdev_get_netif(ifdev)))
		return NULL;

	return ifdev_get_netif(ifdev);
}

/*
 * lwIP IPv6 (source) routing hook.  Since this hook is called only from lwIP's
 * own ip6_route() implementation, this hook must never fire.  If it does,
 * either something is wrong with overriding ip6_route(), or lwIP added other
 * places from which this hook is called.  Both cases are highly problematic
 * and must be resolved somehow, which is why we simply call panic() here.
 */
struct netif *
lwip_hook_ip6_route(const ip6_addr_t * src, const ip6_addr_t * dst)
{

	panic("IPv6 routing hook called - this should not happen!");
}

/*
 * lwIP IPv6 ND6 gateway hook.
 */
const ip6_addr_t *
lwip_hook_nd6_get_gw(struct netif * netif, const ip6_addr_t * ip6addr)
{
	static ip6_addr_t gw_addr; /* may be returned to the caller */
	struct route_entry *route;
	struct ifdev *ifdev;

	ifdev = netif_get_ifdev(netif);
	assert(ifdev != NULL);

	/* Look up the route for the destination IP address. */
	if ((route = route_lookup_v6(ip6addr)) == NULL)
		return NULL;

	/* As for IPv4. */
	if (route->re_ifdev != ifdev) {
		printf("LWIP: unexpected interface for gateway lookup\n");

		return NULL;
	}

	/*
	 * We save memory by storing a packed (zoneless) version of the IPv6
	 * gateway address.  That means we cannot return a pointer to it here.
	 * Instead, we have to resort to expanding the address into a static
	 * variable.  The caller will immediately make a copy anyway, though.
	 */
	if (route->re_flags & RTF_GATEWAY) {
		ip6_addr_copy_from_packed(gw_addr, route->re_gw6);
		ip6_addr_assign_zone(&gw_addr, IP6_UNKNOWN, netif);

		return &gw_addr;
	} else
		return ip6addr;
}

/*
 * Check whether a packet is allowed to be sent to the given destination IPv4
 * address 'ipaddr' on the interface 'ifdev', according to route information.
 * Return TRUE if the packet should be sent.  Return FALSE if the packet should
 * be rejected or discarded, with 'err' set to the error to return to lwIP.
 */
int
route_output_v4(struct ifdev * ifdev, const ip4_addr_t * ipaddr, err_t * err)
{
	const struct route_entry *route;

	/* See if we should reject/blackhole packets to this destination. */
	if (ifdev_is_loopback(ifdev) &&
	    (route = route_lookup_v4(ipaddr)) != NULL &&
	    (route->re_flags & (RTF_REJECT | RTF_BLACKHOLE))) {
		if (route->re_flags & RTF_REJECT)
			*err = ERR_RTE;
		else
			*err = ERR_OK;

		return FALSE;
	}

	return TRUE;
}

/*
 * Check whether a packet is allowed to be sent to the given destination IPv6
 * address 'ipaddr' on the interface 'ifdev', according to route information.
 * Return TRUE if the packet should be sent.  Return FALSE if the packet should
 * be rejected or discarded, with 'err' set to the error to return to lwIP.
 */
int
route_output_v6(struct ifdev * ifdev, const ip6_addr_t * ipaddr, err_t * err)
{
	const struct route_entry *route;

	/* Do one more zone violation test, just in case.  It's cheap. */
	if (ip6_addr_has_zone(ipaddr) &&
	    !ip6_addr_test_zone(ipaddr, ifdev_get_netif(ifdev))) {
		*err = ERR_RTE;

		return FALSE;
	}

	/* See if we should reject/blackhole packets to this destination. */
	if (ifdev_is_loopback(ifdev) &&
	    (route = route_lookup_v6(ipaddr)) != NULL &&
	    (route->re_flags & (RTF_REJECT | RTF_BLACKHOLE))) {
		if (route->re_flags & RTF_REJECT)
			*err = ERR_RTE;
		else
			*err = ERR_OK;

		return FALSE;
	}

	return TRUE;
}
