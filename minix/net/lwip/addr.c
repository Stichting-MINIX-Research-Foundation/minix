/* LWIP service - addr.c - socket address verification and conversion */

#include "lwip.h"

/*
 * Return TRUE if the given socket address is of type AF_UNSPEC, or FALSE
 * otherwise.
 */
int
addr_is_unspec(const struct sockaddr * addr, socklen_t addr_len)
{

	return (addr_len >= offsetof(struct sockaddr, sa_data) &&
	    addr->sa_family == AF_UNSPEC);
}

/*
 * Check whether the given multicast address is generally valid.  This check
 * should not be moved into addr_get_inet(), as we do not want to forbid
 * creating routes for such addresses, for example.  We do however apply the
 * restrictions here to all provided source and destination addresses.  Return
 * TRUE if the address is an acceptable multicast address, or FALSE otherwise.
 */
int
addr_is_valid_multicast(const ip_addr_t * ipaddr)
{
	uint8_t scope;

	assert(ip_addr_ismulticast(ipaddr));

	/* We apply restrictions to IPv6 multicast addresses only. */
	if (IP_IS_V6(ipaddr)) {
		scope = ip6_addr_multicast_scope(ip_2_ip6(ipaddr));

		if (scope == IP6_MULTICAST_SCOPE_RESERVED0 ||
		    scope == IP6_MULTICAST_SCOPE_RESERVEDF)
			return FALSE;

		/*
		 * We do not impose restrictions on the three defined embedded
		 * flags, even though we put no effort into supporting them,
		 * especially in terms of automatically creating routes for
		 * all cases.  We do force the fourth flag to be zero.
		 * Unfortunately there is no lwIP macro to check for this flag.
		 */
		if (ip_2_ip6(ipaddr)->addr[0] & PP_HTONL(0x00800000UL))
			return FALSE;

		/* Prevent KAME-embedded zone IDs from entering the system. */
		if (ip6_addr_has_scope(ip_2_ip6(ipaddr), IP6_UNKNOWN) &&
		    (ip_2_ip6(ipaddr)->addr[0] & PP_HTONL(0x0000ffffUL)))
			return FALSE;
	}

	return TRUE;
}

/*
 * Load a sockaddr structure, as copied from userland, as a lwIP-style IP
 * address and (optionally) a port number.  The expected type of IP address is
 * given as 'type', which must be one of IPADDR_TYPE_{V4,ANY,V6}.  If it is
 * IPADDR_TYPE_V4, 'addr' is expected to point to a sockaddr_in structure.  If
 * it is IPADDR_TYPE_{ANY,V6}, 'addr' is expected to point to a sockaddr_in6
 * structure.  For the _ANY case, the result will be an _ANY address only if it
 * is the unspecified (all-zeroes) address and a _V6 address in all other
 * cases.  For the _V6 case, the result will always be a _V6 address.  The
 * length of the structure pointed to by 'addr' is given as 'addr_len'.  If the
 * boolean 'kame' flag is set, addresses will be interpreted to be KAME style,
 * meaning that for scoped IPv6 addresses, the zone is embedded in the address
 * rather than given in sin6_scope_id.  On success, store the resulting IP
 * address in 'ipaddr'.  If 'port' is not NULL, store the port number in it;
 * otherwise, ignore the port number.  On any parsing failure, return an
 * appropriate negative error code.
 */
int
addr_get_inet(const struct sockaddr * addr, socklen_t addr_len, uint8_t type,
	ip_addr_t * ipaddr, int kame, uint16_t * port)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	ip6_addr_t *ip6addr;
	uint32_t ifindex;

	switch (type) {
	case IPADDR_TYPE_V4:
		if (addr_len != sizeof(sin))
			return EINVAL;

		/*
		 * Getting around strict aliasing problems.  Oh, the irony of
		 * doing an extra memcpy so that the compiler can do a better
		 * job at optimizing..
		 */
		memcpy(&sin, addr, sizeof(sin));

		if (sin.sin_family != AF_INET)
			return EAFNOSUPPORT;

		ip_addr_set_ip4_u32(ipaddr, sin.sin_addr.s_addr);

		if (port != NULL)
			*port = ntohs(sin.sin_port);

		return OK;

	case IPADDR_TYPE_ANY:
	case IPADDR_TYPE_V6:
		if (addr_len != sizeof(sin6))
			return EINVAL;

		/* Again, strict aliasing.. */
		memcpy(&sin6, addr, sizeof(sin6));

		if (sin6.sin6_family != AF_INET6)
			return EAFNOSUPPORT;

		memset(ipaddr, 0, sizeof(*ipaddr));

		/*
		 * This is a bit ugly, but NetBSD does not expose s6_addr32 and
		 * s6_addr is a series of bytes, which is a mismatch for lwIP.
		 * The alternative would be another memcpy..
		 */
		ip6addr = ip_2_ip6(ipaddr);
		assert(sizeof(ip6addr->addr) == sizeof(sin6.sin6_addr));
		memcpy(ip6addr->addr, &sin6.sin6_addr, sizeof(ip6addr->addr));

		/*
		 * If the address may have a scope, extract the zone ID.
		 * Where the zone ID is depends on the 'kame' parameter: KAME-
		 * style addresses have it embedded within the address, whereas
		 * non-KAME addresses use the (misnamed) sin6_scope_id field.
		 */
		if (ip6_addr_has_scope(ip6addr, IP6_UNKNOWN)) {
			if (kame) {
				ifindex =
				    ntohl(ip6addr->addr[0]) & 0x0000ffffUL;

				ip6addr->addr[0] &= PP_HTONL(0xffff0000UL);
			} else {
				/*
				 * Reject KAME-style addresses for normal
				 * socket calls, to save ourselves the trouble
				 * of mixed address styles elsewhere.
				 */
				if (ip6addr->addr[0] & PP_HTONL(0x0000ffffUL))
					return EINVAL;

				ifindex = sin6.sin6_scope_id;
			}

			/*
			 * Reject invalid zone IDs.  This also enforces that
			 * no zone IDs wider than eight bits enter the system.
			 * As a side effect, it is not possible to add routes
			 * for invalid zones, but that should be no problem.
			 */
			if (ifindex != 0 &&
			    ifdev_get_by_index(ifindex) == NULL)
				return ENXIO;

			ip6_addr_set_zone(ip6addr, ifindex);
		} else
			ip6_addr_clear_zone(ip6addr);

		/*
		 * Set the type to ANY if it was ANY and the address itself is
		 * ANY as well.  Otherwise, we are binding to a specific IPv6
		 * address, so IPV6_V6ONLY stops being relevant and we should
		 * leave the address set to V6.  Destination addresses for ANY
		 * are set to V6 elsewhere.
		 */
		if (type == IPADDR_TYPE_ANY && ip6_addr_isany(ip6addr))
			IP_SET_TYPE(ipaddr, type);
		else
			IP_SET_TYPE(ipaddr, IPADDR_TYPE_V6);

		if (port != NULL)
			*port = ntohs(sin6.sin6_port);

		return OK;

	default:
		return EAFNOSUPPORT;
	}
}

/*
 * Store an lwIP-style IP address and port number as a sockaddr structure
 * (sockaddr_in or sockaddr_in6, depending on the given IP address) to be
 * copied to userland.  The result is stored in the buffer pointed to by
 * 'addr'.  Before the call, 'addr_len' must be set to the size of this buffer.
 * This is an internal check to prevent buffer overflows, and must not be used
 * to validate input, since a mismatch will trigger a panic.  After the call,
 * 'addr_len' will be set to the size of the resulting structure.  The lwIP-
 * style address is given as 'ipaddr'.  If the boolean 'kame' flag is set, the
 * address will be stored KAME-style, meaning that for scoped IPv6 addresses,
 * the address zone will be stored embedded in the address rather than in
 * sin6_scope_id.  If relevant, 'port' contains the port number in host-byte
 * order; otherwise it should be set to zone.
 */
void
addr_put_inet(struct sockaddr * addr, socklen_t * addr_len,
	const ip_addr_t * ipaddr, int kame, uint16_t port)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	const ip6_addr_t *ip6addr;
	uint32_t zone;

	switch (IP_GET_TYPE(ipaddr)) {
	case IPADDR_TYPE_V4:
		if (*addr_len < sizeof(sin))
			panic("provided address buffer too small");

		memset(&sin, 0, sizeof(sin));

		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = ip_addr_get_ip4_u32(ipaddr);

		memcpy(addr, &sin, sizeof(sin));
		*addr_len = sizeof(sin);

		break;

	case IPADDR_TYPE_ANY:
	case IPADDR_TYPE_V6:
		if (*addr_len < sizeof(sin6))
			panic("provided address buffer too small");

		ip6addr = ip_2_ip6(ipaddr);

		memset(&sin6, 0, sizeof(sin6));

		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(port);
		memcpy(&sin6.sin6_addr, ip6addr->addr, sizeof(sin6.sin6_addr));

		/*
		 * If the IPv6 address has a zone set, it must be scoped, and
		 * we put the zone in the result.  It may occur that a scoped
		 * IPv6 address does not have a zone here though, for example
		 * if packet routing fails for sendto() with a zoneless address
		 * on an unbound socket, resulting in an RTM_MISS message.  In
		 * such cases, simply leave the zone index blank in the result.
		 */
		if (ip6_addr_has_zone(ip6addr)) {
			assert(ip6_addr_has_scope(ip6addr, IP6_UNKNOWN));

			zone = ip6_addr_zone(ip6addr);
			assert(zone <= UINT8_MAX);

			if (kame)
				sin6.sin6_addr.s6_addr[3] = zone;
			else
				sin6.sin6_scope_id = zone;
		}

		memcpy(addr, &sin6, sizeof(sin6));
		*addr_len = sizeof(sin6);

		break;

	default:
		panic("unknown IP address type: %u", IP_GET_TYPE(ipaddr));
	}
}

/*
 * Load a link-layer sockaddr structure (sockaddr_dl), as copied from userland,
 * and return the contained name and/or hardware address.  The address is
 * provided as 'addr', with length 'addr_len'.  On success, return OK.  If
 * 'name' is not NULL, it must be of size 'name_max', and will be used to store
 * the (null-terminated) interface name in the given structure if present, or
 * the empty string if not.  If 'hwaddr' is not NULL, it will be used to store
 * the hardware address in the given structure, which must in that case be
 * present and exactly 'hwaddr_len' bytes long.  On any parsing failure, return
 * an appropriate negative error code.
 */
int
addr_get_link(const struct sockaddr * addr, socklen_t addr_len, char * name,
	size_t name_max, uint8_t * hwaddr, size_t hwaddr_len)
{
	struct sockaddr_dlx sdlx;
	size_t nlen, alen;

	if (addr_len < offsetof(struct sockaddr_dlx, sdlx_data))
		return EINVAL;

	/*
	 * We cannot prevent callers from passing in massively oversized
	 * sockaddr_dl structure.  However, we insist that all the actual data
	 * be contained within the size of our sockaddr_dlx version.
	 */
	if (addr_len > sizeof(sdlx))
		addr_len = sizeof(sdlx);

	memcpy(&sdlx, addr, addr_len);

	if (sdlx.sdlx_family != AF_LINK)
		return EAFNOSUPPORT;

	/* Address selectors are not currently supported. */
	if (sdlx.sdlx_slen != 0)
		return EINVAL;

	nlen = (size_t)sdlx.sdlx_nlen;
	alen = (size_t)sdlx.sdlx_alen;

	/* The nlen and alen fields are 8-bit, so no risks of overflow here. */
	if (addr_len < offsetof(struct sockaddr_dlx, sdlx_data) + nlen + alen)
		return EINVAL;

	/*
	 * Copy out the name, truncating it if needed.  The name in the
	 * sockaddr is not null terminated, so we have to do that.  If the
	 * sockaddr has no name, copy out an empty name.
	 */
	if (name != NULL) {
		assert(name_max > 0);

		if (name_max > nlen + 1)
			name_max = nlen + 1;

		memcpy(name, sdlx.sdlx_data, name_max - 1);
		name[name_max - 1] = '\0';
	}

	/*
	 * Copy over the hardware address.  For simplicity, we require that the
	 * caller specify the exact hardware address length.
	 */
	if (hwaddr != NULL) {
		if (alen != hwaddr_len)
			return EINVAL;

		memcpy(hwaddr, sdlx.sdlx_data + nlen, hwaddr_len);
	}

	return OK;
}

/*
 * Store a link-layer sockaddr structure (sockaddr_dl), to be copied to
 * userland.  The result is stored in the buffer pointed to by 'addr'.  Before
 * the call, 'addr_len' must be set to the size of this buffer.  This is an
 * internal check to prevent buffer overflows, and must not be used to validate
 * input, since a mismatch will trigger a panic.  After the call, 'addr_len'
 * will be set to the size of the resulting structure.  The given interface
 * index 'ifindex' and (IFT_) interface type 'type' will always be stored in
 * the resulting structure.  If 'name' is not NULL, it must be a null-
 * terminated interface name string which will be included in the structure.
 * If 'hwaddr' is not NULL, it must be a hardware address of length
 * 'hwaddr_len', which will also be included in the structure.
 */
void
addr_put_link(struct sockaddr * addr, socklen_t * addr_len, uint32_t ifindex,
	uint32_t type, const char * name, const uint8_t * hwaddr,
	size_t hwaddr_len)
{
	struct sockaddr_dlx sdlx;
	size_t name_len;
	socklen_t len;

	name_len = (name != NULL) ? strlen(name) : 0;

	if (hwaddr == NULL)
		hwaddr_len = 0;

	assert(name_len < IFNAMSIZ);
	assert(hwaddr_len <= NETIF_MAX_HWADDR_LEN);

	len = offsetof(struct sockaddr_dlx, sdlx_data) + name_len + hwaddr_len;

	if (*addr_len < len)
		panic("provided address buffer too small");

	memset(&sdlx, 0, sizeof(sdlx));
	sdlx.sdlx_len = len;
	sdlx.sdlx_family = AF_LINK;
	sdlx.sdlx_index = ifindex;
	sdlx.sdlx_type = type;
	sdlx.sdlx_nlen = name_len;
	sdlx.sdlx_alen = hwaddr_len;
	if (name_len > 0)
		memcpy(sdlx.sdlx_data, name, name_len);
	if (hwaddr_len > 0)
		memcpy(sdlx.sdlx_data + name_len, hwaddr, hwaddr_len);

	memcpy(addr, &sdlx, len);
	*addr_len = len;
}

/*
 * Convert an IPv4 or IPv6 netmask, given as sockaddr structure 'addr', to a
 * prefix length.  The length of the sockaddr structure is given as 'addr_len'.
 * For consistency with addr_get_inet(), the expected address type is given as
 * 'type', and must be either IPADDR_TYPE_V4 or IPADDR_TYPE_V6.  On success,
 * return OK with the number of set prefix bits returned in 'prefix', and
 * optionally with a lwIP representation of the netmask stored in 'ipaddr' (if
 * not NULL).  On failure, return an appropriate negative error code.  Note
 * that this function does not support compressed IPv4 network masks; such
 * addresses must be expanded before a call to this function.
 */
int
addr_get_netmask(const struct sockaddr * addr, socklen_t addr_len,
	uint8_t type, unsigned int * prefix, ip_addr_t * ipaddr)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	unsigned int byte, bit;
	uint32_t val;

	switch (type) {
	case IPADDR_TYPE_V4:
		if (addr_len != sizeof(sin))
			return EINVAL;

		memcpy(&sin, addr, sizeof(sin));

		if (sin.sin_family != AF_INET)
			return EAFNOSUPPORT;

		val = ntohl(sin.sin_addr.s_addr);

		/* Find the first zero bit. */
		for (bit = 0; bit < IP4_BITS; bit++)
			if (!(val & (1 << (IP4_BITS - bit - 1))))
				break;

		*prefix = bit;

		/* All bits after the first zero bit must also be zero. */
		if (bit < IP4_BITS &&
		    (val & ((1 << (IP4_BITS - bit - 1)) - 1)))
			return EINVAL;

		if (ipaddr != NULL)
			ip_addr_set_ip4_u32(ipaddr, sin.sin_addr.s_addr);

		return OK;

	case IPADDR_TYPE_V6:
		if (addr_len != sizeof(sin6))
			return EINVAL;

		memcpy(&sin6, addr, sizeof(sin6));

		if (sin6.sin6_family != AF_INET6)
			return EAFNOSUPPORT;

		/* Find the first zero bit. */
		for (byte = 0; byte < __arraycount(sin6.sin6_addr.s6_addr);
		    byte++)
			if (sin6.sin6_addr.s6_addr[byte] != 0xff)
				break;

		/* If all bits are set, there is nothing more to do. */
		if (byte == __arraycount(sin6.sin6_addr.s6_addr)) {
			*prefix = __arraycount(sin6.sin6_addr.s6_addr) * NBBY;

			return OK;
		}

		for (bit = 0; bit < NBBY; bit++)
			if (!(sin6.sin6_addr.s6_addr[byte] &
			    (1 << (NBBY - bit - 1))))
				break;

		*prefix = byte * NBBY + bit;

		/* All bits after the first zero bit must also be zero. */
		if (bit < NBBY && (sin6.sin6_addr.s6_addr[byte] &
		    ((1 << (NBBY - bit - 1)) - 1)))
			return EINVAL;

		for (byte++; byte < __arraycount(sin6.sin6_addr.s6_addr);
		    byte++)
			if (sin6.sin6_addr.s6_addr[byte] != 0)
				return EINVAL;

		if (ipaddr != NULL) {
			ip_addr_set_zero_ip6(ipaddr);

			memcpy(ip_2_ip6(ipaddr)->addr, &sin6.sin6_addr,
			    sizeof(ip_2_ip6(ipaddr)->addr));
		}

		return OK;

	default:
		panic("unknown IP address type: %u", type);
	}
}

/*
 * Generate a raw network mask based on the given prefix length.
 */
void
addr_make_netmask(uint8_t * addr, socklen_t addr_len, unsigned int prefix)
{
	unsigned int byte, bit;

	byte = prefix / NBBY;
	bit = prefix % NBBY;

	assert(byte + !!bit <= addr_len);

	if (byte > 0)
		memset(addr, 0xff, byte);
	if (bit != 0)
		addr[byte++] = (uint8_t)(0xff << (NBBY - bit));
	if (byte < addr_len)
		memset(&addr[byte], 0, addr_len - byte);
}

/*
 * Store a network mask as a sockaddr structure, in 'addr'.  Before the call,
 * 'addr_len' must be set to the memory size of 'addr'.  The address type is
 * given as 'type', and must be either IPADDR_TYPE_V4 or IPADDR_TYPE_V6.  The
 * prefix length from which to generate the network mask is given as 'prefix'.
 * Upon return, 'addr_len' is set to the size of the resulting sockaddr
 * structure.
 */
void
addr_put_netmask(struct sockaddr * addr, socklen_t * addr_len, uint8_t type,
	unsigned int prefix)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;

	switch (type) {
	case IPADDR_TYPE_V4:
		if (*addr_len < sizeof(sin))
			panic("provided address buffer too small");

		assert(prefix <= IP4_BITS);

		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;

		addr_make_netmask((uint8_t *)&sin.sin_addr.s_addr,
		    sizeof(sin.sin_addr.s_addr), prefix);

		memcpy(addr, &sin, sizeof(sin));
		*addr_len = sizeof(sin);

		break;

	case IPADDR_TYPE_V6:
		if (*addr_len < sizeof(sin6))
			panic("provided address buffer too small");

		assert(prefix <= IP6_BITS);

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;

		addr_make_netmask(sin6.sin6_addr.s6_addr,
		    sizeof(sin6.sin6_addr.s6_addr), prefix);

		memcpy(addr, &sin6, sizeof(sin6));
		*addr_len = sizeof(sin6);

		break;

	default:
		panic("unknown IP address type: %u", type);
	}
}

/*
 * Normalize the given address in 'src' to the given number of prefix bits,
 * setting all other bits to zero.  Return the result in 'dst'.
 */
void
addr_normalize(ip_addr_t * dst, const ip_addr_t * src, unsigned int prefix)
{
#if !defined(NDEBUG)
	unsigned int addr_len;
#endif /* !defined(NDEBUG) */
	unsigned int byte, bit;
	const uint8_t *srcaddr;
	uint8_t type, *dstaddr;

	type = IP_GET_TYPE(src);

	memset(dst, 0, sizeof(*dst));
	IP_SET_TYPE(dst, type);

	switch (type) {
	case IPADDR_TYPE_V4:
		srcaddr = (const uint8_t *)&ip_2_ip4(src)->addr;
		dstaddr = (uint8_t *)&ip_2_ip4(dst)->addr;
#if !defined(NDEBUG)
		addr_len = sizeof(ip_2_ip4(src)->addr);
#endif /* !defined(NDEBUG) */

		break;

	case IPADDR_TYPE_V6:
		ip6_addr_set_zone(ip_2_ip6(dst), ip6_addr_zone(ip_2_ip6(src)));

		srcaddr = (const uint8_t *)&ip_2_ip6(src)->addr;
		dstaddr = (uint8_t *)&ip_2_ip6(dst)->addr;
#if !defined(NDEBUG)
		addr_len = sizeof(ip_2_ip6(src)->addr);
#endif /* !defined(NDEBUG) */

		break;

	default:
		panic("unknown IP address type: %u", type);
	}

	byte = prefix / NBBY;
	bit = prefix % NBBY;

	assert(byte + !!bit <= addr_len);

	if (byte > 0)
		memcpy(dstaddr, srcaddr, byte);
	if (bit != 0) {
		dstaddr[byte] =
		    srcaddr[byte] & (uint8_t)(0xff << (NBBY - bit));
		byte++;
	}
}

/*
 * Return the number of common bits between the given two addresses, up to the
 * given maximum.  Thus, return a value between 0 and 'max' inclusive.
 */
unsigned int
addr_get_common_bits(const ip_addr_t * ipaddr1, const ip_addr_t * ipaddr2,
	unsigned int max)
{
	unsigned int addr_len, prefix, bit;
	const uint8_t *addr1, *addr2;
	uint8_t byte;

	switch (IP_GET_TYPE(ipaddr1)) {
	case IPADDR_TYPE_V4:
		assert(IP_IS_V4(ipaddr2));

		addr1 = (const uint8_t *)&ip_2_ip4(ipaddr1)->addr;
		addr2 = (const uint8_t *)&ip_2_ip4(ipaddr2)->addr;
		addr_len = sizeof(ip_2_ip4(ipaddr1)->addr);

		break;

	case IPADDR_TYPE_V6:
		assert(IP_IS_V6(ipaddr2));

		addr1 = (const uint8_t *)&ip_2_ip6(ipaddr1)->addr;
		addr2 = (const uint8_t *)&ip_2_ip6(ipaddr2)->addr;
		addr_len = sizeof(ip_2_ip6(ipaddr1)->addr);

		break;

	default:
		panic("unknown IP address type: %u", IP_GET_TYPE(ipaddr1));
	}

	if (addr_len > max * NBBY)
		addr_len = max * NBBY;

	prefix = 0;

	for (prefix = 0; addr_len > 0; addr1++, addr2++, prefix += NBBY) {
		if ((byte = (*addr1 ^ *addr2)) != 0) {
			/* TODO: see if we want a lookup table for this. */
			for (bit = 0; bit < NBBY; bit++, prefix++)
				if (byte & (1 << (NBBY - bit - 1)))
					break;
			break;
		}
	}

	if (prefix > max)
		prefix = max;

	return prefix;
}

/*
 * Convert the given IPv4 address to an IPv4-mapped IPv6 address.
 */
void
addr_make_v4mapped_v6(ip_addr_t * dst, const ip4_addr_t * src)
{

	IP_ADDR6(dst, 0, 0, PP_HTONL(0x0000ffffUL), ip4_addr_get_u32(src));
}
