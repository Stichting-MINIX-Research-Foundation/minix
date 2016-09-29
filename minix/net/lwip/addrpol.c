/* LWIP service - addrpol.c - address policy table and values */
/*
 * The main purpose of this module is to implement the address policy table
 * described in RFC 6724.  In general, the policy table is used for two
 * purposes: source address selection, which is part of this service, and
 * destination address selection, which is implemented in libc.  NetBSD 7, the
 * version that MINIX 3 is synced against at this moment, does not actually
 * implement the libc part yet, though.  That will change with NetBSD 8, where
 * libc uses sysctl(7) to obtain the kernel's policy table, which itself can be
 * changed with the new ip6addrctl(8) utility.  Once we resync to NetBSD 8, we
 * will also have to support this new functionality, and this module is where
 * it would be implemented.  Since NetBSD 7 is even lacking the necessary
 * definitions, we cannot do that ahead of time, though.  Thus, until then,
 * this module is rather simple, as it only implements a static policy table
 * used for source address selection.  No changes beyond this module should be
 * necessary, e.g. we are purposely not caching labels for local addresses.
 */

#include "lwip.h"

/*
 * Address policy table.  Currently hardcoded to the default of RFC 6724.
 * Sorted by prefix length, so that the first match is always also the longest.
 */
static const struct {
	ip_addr_t ipaddr;
	unsigned int prefix;
	int precedence;
	int label;
} addrpol_table[] = {
	{ IPADDR6_INIT_HOST(0, 0, 0, 1),		128, 50,  0 },
	{ IPADDR6_INIT_HOST(0, 0, 0x0000ffffUL, 0),	 96, 35,  4 },
	{ IPADDR6_INIT_HOST(0, 0, 0, 0),		 96,  1,  3 },
	{ IPADDR6_INIT_HOST(0x20010000UL, 0, 0, 0),	 32,  5,  5 },
	{ IPADDR6_INIT_HOST(0x20020000UL, 0, 0, 0),	 16, 30,  2 },
	{ IPADDR6_INIT_HOST(0x3ffe0000UL, 0, 0, 0),	 16,  1, 12 },
	{ IPADDR6_INIT_HOST(0xfec00000UL, 0, 0, 0),	 10,  1, 11 },
	{ IPADDR6_INIT_HOST(0xfc000000UL, 0, 0, 0),	  7,  3, 13 },
	{ IPADDR6_INIT_HOST(0, 0, 0, 0),		  0, 40,  1 }
};

/*
 * Obtain the label value for the given IP address from the address policy
 * table.  Currently only IPv6 addresses may be given.  This function is linear
 * in number of address policy table entries, requiring a relatively expensive
 * normalization operation for each entry, so it should not be called lightly.
 * Its results should not be cached beyond local contexts either, because the
 * policy table itself may be changed from userland (in the future).
 *
 * TODO: convert IPv4 addresses to IPv4-mapped IPv6 addresses.
 * TODO: embed the interface index in link-local addresses.
 */
int
addrpol_get_label(const ip_addr_t * iporig)
{
	ip_addr_t ipaddr;
	unsigned int i;

	assert(IP_IS_V6(iporig));

	/*
	 * The policy table is sorted by prefix length such that the first
	 * match is also the one with the longest prefix, and as such the best.
	 */
	for (i = 0; i < __arraycount(addrpol_table); i++) {
		addr_normalize(&ipaddr, iporig, addrpol_table[i].prefix);

		if (ip_addr_cmp(&addrpol_table[i].ipaddr, &ipaddr))
			return addrpol_table[i].label;
	}

	/*
	 * We cannot possibly get here with the default policy table, because
	 * the last entry will always match.  It is not clear what we should
	 * return if there is no matching entry, though.  For now, we return
	 * the default label value for the default (::/0) entry, which is 1.
	 */
	return 1;
}

/*
 * Return an opaque positive value (possibly zero) that represents the scope of
 * the given IP address.  A larger value indicates a wider scope.  The 'is_src'
 * flag indicates whether the address is a source or a destination address,
 * which affects the value returned for unknown addresses.  A scope is a direct
 * function of only the given address, so the result may be cached on a per-
 * address basis without risking invalidation at any point in time.
 */
int
addrpol_get_scope(const ip_addr_t * ipaddr, int is_src)
{
	const ip6_addr_t *ip6addr;

	/*
	 * For now, all IPv4 addresses are considered global.  This function is
	 * currently called only for IPv6 addresses anyway.
	 */
	if (IP_IS_V4(ipaddr))
		return IP6_MULTICAST_SCOPE_GLOBAL;

	assert(IP_IS_V6(ipaddr));

	ip6addr = ip_2_ip6(ipaddr);

	/*
	 * These are ordered not by ascending scope, but (roughly) by expected
	 * likeliness to match, for performance reasons.
	 */
	if (ip6_addr_isglobal(ip6addr))
		return IP6_MULTICAST_SCOPE_GLOBAL;

	if (ip6_addr_islinklocal(ip6addr) || ip6_addr_isloopback(ip6addr))
		return IP6_MULTICAST_SCOPE_LINK_LOCAL;

	/*
	 * We deliberately deviate from RFC 6724 Sec. 3.1 by considering
	 * Unique-Local Addresses (ULAs) to be of smaller scope than global
	 * addresses, to avoid that during source address selection, a
	 * preferred ULA is picked over a deprecated global address when given
	 * a global address as destination, as that would likely result in
	 * broken two-way communication.
	 */
	if (ip6_addr_isuniquelocal(ip6addr))
		return IP6_MULTICAST_SCOPE_ORGANIZATION_LOCAL;

	if (ip6_addr_ismulticast(ip6addr))
		return ip6_addr_multicast_scope(ip6addr);

	/* Site-local addresses are deprecated. */
	if (ip6_addr_issitelocal(ip6addr))
		return IP6_MULTICAST_SCOPE_SITE_LOCAL;

	/*
	 * If the address is a source address, give it a scope beyond global to
	 * make sure that a "real" global address is picked first.  If the
	 * address is a destination address, give it a global scope so as to
	 * pick "real" global addresses over unknown-scope source addresses.
	 */
	if (is_src)
		return IP6_MULTICAST_SCOPE_RESERVEDF; /* greater than GLOBAL */
	else
		return IP6_MULTICAST_SCOPE_GLOBAL;
}
