/* LWIP service - ipsock.c - shared IP-level socket code */

#include "lwip.h"
#include "ifaddr.h"

#define ip6_hdr __netbsd_ip6_hdr	/* conflicting definitions */
#include <net/route.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#undef ip6_hdr

/* The following are sysctl(7) settings. */
int lwip_ip4_forward = 0;		/* We patch lwIP to check these.. */
int lwip_ip6_forward = 0;		/*  ..two settings at run time.   */
static int ipsock_v6only = 1;

/* The CTL_NET PF_INET IPPROTO_IP subtree. */
static struct rmib_node net_inet_ip_table[] = {
/* 1*/	[IPCTL_FORWARDING]	= RMIB_INTPTR(RMIB_RW, &lwip_ip4_forward,
				    "forwarding",
				    "Enable forwarding of INET diagrams"),
/* 3*/	[IPCTL_DEFTTL]		= RMIB_INT(RMIB_RO, IP_DEFAULT_TTL, "ttl",
				    "Default TTL for an INET diagram"),
/*23*/	[IPCTL_LOOPBACKCKSUM]	= RMIB_FUNC(RMIB_RW | CTLTYPE_INT, sizeof(int),
				    loopif_cksum, "do_loopback_cksum",
				    "Perform IP checksum on loopback"),
};

static struct rmib_node net_inet_ip_node =
    RMIB_NODE(RMIB_RO, net_inet_ip_table, "ip", "IPv4 related settings");

/* The CTL_NET PF_INET6 IPPROTO_IPV6 subtree. */
static struct rmib_node net_inet6_ip6_table[] = {
/* 1*/	[IPV6CTL_FORWARDING]	= RMIB_INTPTR(RMIB_RW, &lwip_ip6_forward,
				    "forwarding",
				    "Enable forwarding of INET6 diagrams"),
				/*
				 * The following functionality is not
				 * implemented in lwIP at this time.
				 */
/* 2*/	[IPV6CTL_SENDREDIRECTS]	= RMIB_INT(RMIB_RO, 0, "redirect", "Enable "
				    "sending of ICMPv6 redirect messages"),
/* 3*/	[IPV6CTL_DEFHLIM]	= RMIB_INT(RMIB_RO, IP_DEFAULT_TTL, "hlim",
				    "Hop limit for an INET6 datagram"),
/*12*/	[IPV6CTL_ACCEPT_RTADV]	= RMIB_INTPTR(RMIB_RW, &ifaddr_accept_rtadv,
				    "accept_rtadv",
				    "Accept router advertisements"),
/*16*/	[IPV6CTL_DAD_COUNT]	= RMIB_INT(RMIB_RO,
				    LWIP_IPV6_DUP_DETECT_ATTEMPTS, "dad_count",
				    "Number of Duplicate Address Detection "
				    "probes to send"),
/*24*/	[IPV6CTL_V6ONLY]	= RMIB_INTPTR(RMIB_RW, &ipsock_v6only,
				    "v6only", "Disallow PF_INET6 sockets from "
				    "connecting to PF_INET sockets"),
				/*
				 * The following setting is significantly
				 * different from NetBSD, and therefore it has
				 * a somewhat different description as well.
				 */
/*35*/	[IPV6CTL_AUTO_LINKLOCAL]= RMIB_INTPTR(RMIB_RW, &ifaddr_auto_linklocal,
				    "auto_linklocal", "Enable global support "
				    "for adding IPv6link-local addresses to "
				    "interfaces"),
				/*
				 * Temporary addresses are managed entirely by
				 * userland.  We only maintain the settings.
				 */
/*+0*/	[IPV6CTL_MAXID]		= RMIB_INT(RMIB_RW, 0, "use_tempaddr",
				    "Use temporary address"),
/*+1*/	[IPV6CTL_MAXID + 1]	= RMIB_INT(RMIB_RW, 86400, "temppltime",
				    "Preferred lifetime of a temporary "
				    "address"),
/*+2*/	[IPV6CTL_MAXID + 2]	= RMIB_INT(RMIB_RW, 604800, "tempvltime",
				    "Valid lifetime of a temporary address"),
};

static struct rmib_node net_inet6_ip6_node =
    RMIB_NODE(RMIB_RO, net_inet6_ip6_table, "ip6", "IPv6 related settings");

/*
 * Initialize the IP sockets module.
 */
void
ipsock_init(void)
{

	/*
	 * Register the net.inet.ip and net.inet6.ip6 subtrees.  Unlike for the
	 * specific protocols (TCP/UDP/RAW), here the IPv4 and IPv6 subtrees
	 * are and must be separate, even though many settings are shared
	 * between the two at the lwIP level.  Ultimately we may have to split
	 * the subtrees for the specific protocols, too, though..
	 */
	mibtree_register_inet(AF_INET, IPPROTO_IP, &net_inet_ip_node);
	mibtree_register_inet(AF_INET6, IPPROTO_IPV6, &net_inet6_ip6_node);
}

/*
 * Return the lwIP IP address type (IPADDR_TYPE_) for the given IP socket.
 */
static int
ipsock_get_type(struct ipsock * ip)
{

	if (!(ip->ip_flags & IPF_IPV6))
		return IPADDR_TYPE_V4;
	else if (ip->ip_flags & IPF_V6ONLY)
		return IPADDR_TYPE_V6;
	else
		return IPADDR_TYPE_ANY;
}

/*
 * Create an IP socket, for the given (PF_/AF_) domain and initial send and
 * receive buffer sizes.  Return the lwIP IP address type that should be used
 * to create the corresponding PCB.  Return a pointer to the libsockevent
 * socket in 'sockp'.  This function must not allocate any resources in any
 * form, as socket creation may still fail later, in which case no destruction
 * function is called.
 */
int
ipsock_socket(struct ipsock * ip, int domain, size_t sndbuf, size_t rcvbuf,
	struct sock ** sockp)
{

	ip->ip_flags = (domain == AF_INET6) ? IPF_IPV6 : 0;

	if (domain == AF_INET6 && ipsock_v6only)
		ip->ip_flags |= IPF_V6ONLY;

	ip->ip_sndbuf = sndbuf;
	ip->ip_rcvbuf = rcvbuf;

	/* Important: when adding settings here, also change ipsock_clone(). */

	*sockp = &ip->ip_sock;

	return ipsock_get_type(ip);
}

/*
 * Clone the given socket 'ip' into the new socket 'newip', using the socket
 * identifier 'newid'.  In particular, tell libsockevent about the clone and
 * copy over any settings from 'ip' to 'newip' that can be inherited on a
 * socket.  Cloning is used for new TCP connections arriving on listening TCP
 * sockets.  This function must not fail.
 */
void
ipsock_clone(struct ipsock * ip, struct ipsock * newip, sockid_t newid)
{

	sockevent_clone(&ip->ip_sock, &newip->ip_sock, newid);

	/* Inherit all settings from the original socket. */
	newip->ip_flags = ip->ip_flags;
	newip->ip_sndbuf = ip->ip_sndbuf;
	newip->ip_rcvbuf = ip->ip_rcvbuf;
}

/*
 * Create an <any> address for the given socket, taking into account whether
 * the socket is IPv4, IPv6, or mixed.  The generated address, stored in
 * 'ipaddr', will have the same type as returned from the ipsock_socket() call.
 */
void
ipsock_get_any_addr(struct ipsock * ip, ip_addr_t * ipaddr)
{

	ip_addr_set_any(ipsock_is_ipv6(ip), ipaddr);

	if (ipsock_is_ipv6(ip) && !ipsock_is_v6only(ip))
		IP_SET_TYPE(ipaddr, IPADDR_TYPE_ANY);
}

/*
 * Verify whether the given (properly scoped) IP address is a valid source
 * address for the given IP socket.  The 'allow_mcast' flag indicates whether
 * the source address is allowed to be a multicast address.  Return OK on
 * success.  If 'ifdevp' is not NULL, it is filled with either the interface
 * that owns the address, or NULL if the address is (while valid) not
 * associated with a particular interface.  On failure, return a negative error
 * code.  This function must be called, in one way or another, for every source
 * address used for binding or sending on a IP-layer socket.
 */
int
ipsock_check_src_addr(struct ipsock * ip, ip_addr_t * ipaddr, int allow_mcast,
	struct ifdev ** ifdevp)
{
	ip6_addr_t *ip6addr;
	struct ifdev *ifdev;
	uint32_t inaddr, zone;
	int is_mcast;

	/*
	 * TODO: for now, forbid binding to multicast addresses.  Callers that
	 * never allow multicast addresses anyway (e.g., IPV6_PKTINFO) should
	 * do their own check for this; the one here may eventually be removed.
	 */
	is_mcast = ip_addr_ismulticast(ipaddr);

	if (is_mcast && !allow_mcast)
		return EADDRNOTAVAIL;

	if (IP_IS_V6(ipaddr)) {
		/*
		 * The given address must not have a KAME-style embedded zone.
		 * This check is already performed in addr_get_inet(), but we
		 * have to replicate it here because not all source addresses
		 * go through addr_get_inet().
		 */
		ip6addr = ip_2_ip6(ipaddr);

		if (ip6_addr_has_scope(ip6addr, IP6_UNKNOWN) &&
		    (ip6addr->addr[0] & PP_HTONL(0x0000ffffUL)))
			return EINVAL;

		/*
		 * lwIP does not support IPv4-mapped IPv6 addresses, so these
		 * must be converted to plain IPv4 addresses instead.  The IPv4
		 * 'any' address is not supported in this form.  In V6ONLY
		 * mode, refuse connecting or sending to IPv4-mapped addresses
		 * at all.
		 */
		if (ip6_addr_isipv4mappedipv6(ip6addr)) {
			if (ipsock_is_v6only(ip))
				return EINVAL;

			inaddr = ip6addr->addr[3];

			if (inaddr == PP_HTONL(INADDR_ANY))
				return EADDRNOTAVAIL;

			ip_addr_set_ip4_u32(ipaddr, inaddr);
		}
	}

	ifdev = NULL;

	if (!ip_addr_isany(ipaddr)) {
		if (IP_IS_V6(ipaddr) &&
		    ip6_addr_lacks_zone(ip_2_ip6(ipaddr), IP6_UNKNOWN))
			return EADDRNOTAVAIL;

		/*
		 * If the address is a unicast address, it must be assigned to
		 * an interface.  Otherwise, if it is a zoned multicast
		 * address, the zone denotes the interface.  For global
		 * multicast addresses, we cannot determine an interface.
		 */
		if (!is_mcast) {
			if ((ifdev = ifaddr_map_by_addr(ipaddr)) == NULL)
				return EADDRNOTAVAIL;
		} else {
			/* Some multicast addresses are not acceptable. */
			if (!addr_is_valid_multicast(ipaddr))
				return EINVAL;

			if (IP_IS_V6(ipaddr) &&
			    ip6_addr_has_zone(ip_2_ip6(ipaddr))) {
				zone = ip6_addr_zone(ip_2_ip6(ipaddr));

				if ((ifdev = ifdev_get_by_index(zone)) == NULL)
					return ENXIO;
			}
		}
	}

	if (ifdevp != NULL)
		*ifdevp = ifdev;

	return OK;
}

/*
 * Retrieve and validate a source address for use in a socket bind call on
 * socket 'ip'.  The user-provided address is given as 'addr', with length
 * 'addr_len'.  The socket's current local IP address and port are given as
 * 'local_ip' and 'local_port', respectively; for raw sockets, the given local
 * port number is always zero.  The caller's endpoint is given as 'user_endpt',
 * used to make sure only root can bind to local port numbers.  The boolean
 * 'allow_mcast' flag indicates whether the source address is allowed to be a
 * multicast address.  On success, return OK with the source IP address stored
 * in 'src_addr' and, if 'src_port' is not NULL, the port number to bind to
 * stored in 'portp'.  Otherwise, return a negative error code.  This function
 * performs all the tasks necessary before the socket can be bound using a lwIP
 * call.
 */
int
ipsock_get_src_addr(struct ipsock * ip, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt, ip_addr_t * local_ip,
	uint16_t local_port, int allow_mcast, ip_addr_t * src_addr,
	uint16_t * src_port)
{
	uint16_t port;
	int r;

	/*
	 * If the socket has been bound already, it cannot be bound again.
	 * We check this by checking whether the current local port is non-
	 * zero.  This rule does not apply to raw sockets, but raw sockets have
	 * no port numbers anyway, so this conveniently works out.  However,
	 * raw sockets may not be rebound after being connected, but that is
	 * checked before we even get here.
	 */
	if (local_port != 0)
		return EINVAL;

	/* Parse the user-provided address. */
	if ((r = addr_get_inet(addr, addr_len, ipsock_get_type(ip), src_addr,
	    FALSE /*kame*/, &port)) != OK)
		return r;

	/* Validate the user-provided address. */
	if ((r = ipsock_check_src_addr(ip, src_addr, allow_mcast,
	    NULL /*ifdevp*/)) != OK)
		return r;

	/*
	 * If we are interested in port numbers at all (for non-raw sockets,
	 * meaning portp is not NULL), make sure that only the superuser can
	 * bind to privileged port numbers.  For raw sockets, only the
	 * superuser can open a socket anyway, so we need no check here.
	 */
	if (src_port != NULL) {
		if (port != 0 && port < IPPORT_RESERVED &&
		    !util_is_root(user_endpt))
			return EACCES;

		*src_port = port;
	}

	return OK;
}

/*
 * Retrieve and validate a destination address for use in a socket connect or
 * sendto call.  The user-provided address is given as 'addr', with length
 * 'addr_len'.  The socket's current local IP address is given as 'local_addr'.
 * On success, return OK with the destination IP address stored in 'dst_addr'
 * and, if 'dst_port' is not NULL, the port number to bind to stored in
 * 'dst_port'.  Otherwise, return a negative error code.  This function must be
 * called, in one way or another, for every destination address used for
 * connecting or sending on a IP-layer socket.
 */
int
ipsock_get_dst_addr(struct ipsock * ip, const struct sockaddr * addr,
	socklen_t addr_len, const ip_addr_t * local_addr, ip_addr_t * dst_addr,
	uint16_t * dst_port)
{
	uint16_t port;
	int r;

	/* Parse the user-provided address. */
	if ((r = addr_get_inet(addr, addr_len, ipsock_get_type(ip), dst_addr,
	    FALSE /*kame*/, &port)) != OK)
		return r;

	/* Destination addresses are always specific. */
	if (IP_GET_TYPE(dst_addr) == IPADDR_TYPE_ANY)
		IP_SET_TYPE(dst_addr, IPADDR_TYPE_V6);

	/*
	 * lwIP does not support IPv4-mapped IPv6 addresses, so these must be
	 * supported to plain IPv4 addresses instead.  In V6ONLY mode, refuse
	 * connecting or sending to IPv4-mapped addresses at all.
	 */
	if (IP_IS_V6(dst_addr) &&
	    ip6_addr_isipv4mappedipv6(ip_2_ip6(dst_addr))) {
		if (ipsock_is_v6only(ip))
			return EINVAL;

		ip_addr_set_ip4_u32(dst_addr, ip_2_ip6(dst_addr)->addr[3]);
	}

	/*
	 * Now make sure that the local and remote addresses are of the same
	 * family.  The local address may be of type IPADDR_TYPE_ANY, which is
	 * allowed for both IPv4 and IPv6.  Even for connectionless socket
	 * types we must perform this check as part of connect calls (as well
	 * as sendto calls!) because otherwise we will create problems for
	 * sysctl based socket enumeration (i.e., netstat), which uses the
	 * local IP address type to determine the socket family.
	 */
	if (IP_GET_TYPE(local_addr) != IPADDR_TYPE_ANY &&
	    IP_IS_V6(local_addr) != IP_IS_V6(dst_addr))
		return EINVAL;

	/*
	 * TODO: on NetBSD, an 'any' destination address is replaced with a
	 * local interface address.
	 */
	if (ip_addr_isany(dst_addr))
		return EHOSTUNREACH;

	/*
	 * If the address is a multicast address, the multicast address itself
	 * must be valid.
	 */
	if (ip_addr_ismulticast(dst_addr) &&
	    !addr_is_valid_multicast(dst_addr))
		return EINVAL;

	/*
	 * TODO: decide whether to add a zone to a scoped IPv6 address that
	 * lacks a zone.  For now, we let lwIP handle this, as lwIP itself
	 * will always add the zone at some point.  If anything changes there,
	 * this would be the place to set the zone (using a route lookup).
	 */

	/*
	 * For now, we do not forbid or alter any other particular destination
	 * addresses.
	 */

	if (dst_port != NULL) {
		/*
		 * Disallow connecting/sending to port zero.  There is no error
		 * code that applies well to this case, so we copy NetBSD's.
		 */
		if (port == 0)
			return EADDRNOTAVAIL;

		*dst_port = port;
	}

	return OK;
}

/*
 * Store the address 'ipaddr' associated with the socket 'ip' (for example, it
 * may be the local or remote IP address of the socket) as a sockaddr structure
 * in 'addr'.  A port number is provided as 'port' (in host-byte order) if
 * relevant, and zero is passed in otherwise.  This function MUST only be
 * called from contexts where 'addr' is a buffer provided by libsockevent or
 * libsockdriver, meaning that it is of size SOCKADDR_MAX.  The value pointed
 * to by 'addr_len' is not expected to be initialized in calls to this function
 * (and will typically zero).  On return, 'addr_len' is filled with the length
 * of the address generated in 'addr'.  This function never fails.
 */
void
ipsock_put_addr(struct ipsock * ip, struct sockaddr * addr,
	socklen_t * addr_len, ip_addr_t * ipaddr, uint16_t port)
{
	ip_addr_t mappedaddr;

	/*
	 * If the socket is an AF_INET6-type socket, and the given address is
	 * an IPv4-type address, store it as an IPv4-mapped IPv6 address.
	 */
	if (ipsock_is_ipv6(ip) && IP_IS_V4(ipaddr)) {
		addr_make_v4mapped_v6(&mappedaddr, ip_2_ip4(ipaddr));

		ipaddr = &mappedaddr;
	}

	/*
	 * We have good reasons to keep the sockdriver and sockevent APIs as
	 * they are, namely, defaulting 'addr_len' to zero such that the caller
	 * must provide a non-zero length (only) when returning a valid
	 * address.  The consequence here is that we have to know the size of
	 * the provided buffer.  For libsockevent callbacks, we are always
	 * guaranteed to get a buffer of at least this size.
	 */
	*addr_len = SOCKADDR_MAX;

	addr_put_inet(addr, addr_len, ipaddr, FALSE /*kame*/, port);
}

/*
 * Set socket options on an IP socket.
 */
int
ipsock_setsockopt(struct ipsock * ip, int level, int name,
	const struct sockdriver_data * data, socklen_t len,
	struct ipopts * ipopts)
{
	int r, val, allow;
	uint8_t type;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_SNDBUF:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val <= 0 || (size_t)val < ipopts->sndmin ||
			    (size_t)val > ipopts->sndmax)
				return EINVAL;

			ip->ip_sndbuf = val;

			return OK;

		case SO_RCVBUF:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val <= 0 || (size_t)val < ipopts->rcvmin ||
			    (size_t)val > ipopts->rcvmax)
				return EINVAL;

			ip->ip_rcvbuf = val;

			return OK;
		}

		break;

	case IPPROTO_IP:
		if (ipsock_is_ipv6(ip))
			break;

		switch (name) {
		case IP_TOS:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < 0 || val > UINT8_MAX)
				return EINVAL;

			*ipopts->tos = (uint8_t)val;

			return OK;

		case IP_TTL:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < 0 || val > UINT8_MAX)
				return EINVAL;

			*ipopts->ttl = (uint8_t)val;

			return OK;
		}

		break;

	case IPPROTO_IPV6:
		if (!ipsock_is_ipv6(ip))
			break;

		switch (name) {
		case IPV6_UNICAST_HOPS:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < -1 || val > UINT8_MAX)
				return EINVAL;

			if (val == -1)
				val = IP_DEFAULT_TTL;

			*ipopts->ttl = val;

			return OK;

		case IPV6_TCLASS:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < -1 || val > UINT8_MAX)
				return EINVAL;

			if (val == -1)
				val = 0;

			*ipopts->tos = val;

			return OK;

		case IPV6_V6ONLY:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			/*
			 * If the socket has been bound to an actual address,
			 * we still allow the option to be changed, but it no
			 * longer has any effect.
			 */
			type = IP_GET_TYPE(ipopts->local_ip);
			allow = (type == IPADDR_TYPE_ANY ||
			    (type == IPADDR_TYPE_V6 &&
			    ip_addr_isany(ipopts->local_ip)));

			if (val) {
				ip->ip_flags |= IPF_V6ONLY;

				type = IPADDR_TYPE_V6;
			} else {
				ip->ip_flags &= ~IPF_V6ONLY;

				type = IPADDR_TYPE_ANY;
			}

			if (allow)
				IP_SET_TYPE(ipopts->local_ip, type);

			return OK;
		}

		break;
	}

	return ENOPROTOOPT;
}

/*
 * Retrieve socket options on an IP socket.
 */
int
ipsock_getsockopt(struct ipsock * ip, int level, int name,
	const struct sockdriver_data * data, socklen_t * len,
	struct ipopts * ipopts)
{
	int val;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_SNDBUF:
			val = ip->ip_sndbuf;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_RCVBUF:
			val = ip->ip_rcvbuf;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;

	case IPPROTO_IP:
		if (ipsock_is_ipv6(ip))
			break;

		switch (name) {
		case IP_TOS:
			val = (int)*ipopts->tos;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IP_TTL:
			val = (int)*ipopts->ttl;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;

	case IPPROTO_IPV6:
		if (!ipsock_is_ipv6(ip))
			break;

		switch (name) {
		case IPV6_UNICAST_HOPS:
			val = *ipopts->ttl;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_TCLASS:
			val = *ipopts->tos;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_V6ONLY:
			val = !!(ip->ip_flags & IPF_V6ONLY);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;
	}

	return ENOPROTOOPT;
}

/*
 * Fill the given kinfo_pcb sysctl(7) structure with IP-level information.
 */
void
ipsock_get_info(struct kinfo_pcb * ki, const ip_addr_t * local_ip,
	uint16_t local_port, const ip_addr_t * remote_ip, uint16_t remote_port)
{
	ip_addr_t ipaddr;
	socklen_t len;
	uint8_t type;

	len = sizeof(ki->ki_spad); /* use this for the full size, not ki_src */

	addr_put_inet(&ki->ki_src, &len, local_ip, TRUE /*kame*/, local_port);

	/*
	 * At this point, the local IP address type has already been used to
	 * determine whether this is an IPv4 or IPv6 socket.  While not ideal,
	 * that is the best we can do: we cannot use IPv4-mapped IPv6 addresses
	 * in lwIP PCBs, we cannot store the original type in those PCBs, and
	 * we also cannot rely on the PCB having an associated ipsock object
	 * anymore.  We also cannot use the ipsock only when present: it could
	 * make a TCP PCB "jump" from IPv6 to IPv4 in the netstat listing when
	 * it goes into TIME_WAIT state, for example.
	 *
	 * So, use *only* the type of the local IP address to determine whether
	 * this is an IPv4 or an IPv6 socket.  At the same time, do *not* rely
	 * on the remote IP address being IPv4 for a local IPv4 address; it may
	 * be of type IPADDR_TYPE_V6 for an unconnected socket bound to an
	 * IPv4-mapped IPv6 address.  Pretty messy, but we're limited by what
	 * lwIP offers here.  Since it's just netstat, it need not be perfect.
	 */
	if ((type = IP_GET_TYPE(local_ip)) == IPADDR_TYPE_V4) {
		if (!ip_addr_isany(local_ip) || local_port != 0)
			ki->ki_prstate = INP_BOUND;

		/*
		 * Make sure the returned socket address types are consistent.
		 * The only case where the remote IP address is not IPv4 here
		 * is when it is not set yet, so there is no need to check
		 * whether it is the 'any' address: it always is.
		 */
		if (IP_GET_TYPE(remote_ip) != IPADDR_TYPE_V4) {
			ip_addr_set_zero_ip4(&ipaddr);

			remote_ip = &ipaddr;
		}
	} else {
		if (!ip_addr_isany(local_ip) || local_port != 0)
			ki->ki_prstate = IN6P_BOUND;
		if (type != IPADDR_TYPE_ANY)
			ki->ki_pflags |= IN6P_IPV6_V6ONLY;
	}

	len = sizeof(ki->ki_dpad); /* use this for the full size, not ki_dst */

	addr_put_inet(&ki->ki_dst, &len, remote_ip, TRUE /*kame*/,
	    remote_port);

	/* Check the type of the *local* IP address here.  See above. */
	if (!ip_addr_isany(remote_ip) || remote_port != 0) {
		if (type == IPADDR_TYPE_V4)
			ki->ki_prstate = INP_CONNECTED;
		else
			ki->ki_prstate = IN6P_CONNECTED;
	}
}
