/* LWIP service - rawsock.c - RAW sockets */
/*
 * For IPv6 sockets, this module attempts to implement a part of RFC 3542, but
 * currently not more than what is supported by lwIP and/or what is expected by
 * a handful of standard utilities (dhcpcd, ping6, traceroute6..).
 *
 * For general understanding, be aware that IPv4 raw sockets always receive
 * packets including the IP header, and may be used to send packets including
 * the IP header if IP_HDRINCL is set, while IPv6 raw sockets always send and
 * receive actual payloads only, using ancillary (control) data to set and
 * retrieve per-packet IP header fields.
 *
 * For packet headers we follow general BSD semantics.  For example, some IPv4
 * header fields are swapped both when sending and when receiving.  Also, like
 * on NetBSD, IPPROTO_RAW is not a special value in any way.
 */

#include "lwip.h"
#include "ifaddr.h"
#include "pktsock.h"

#include "lwip/raw.h"
#include "lwip/inet_chksum.h"

#include <net/route.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

/* The number of RAW sockets.  Inherited from the lwIP configuration. */
#define NR_RAWSOCK	MEMP_NUM_RAW_PCB

/*
 * Outgoing packets are not getting buffered, so the send buffer size simply
 * determines the maximum size for sent packets.  The send buffer maximum is
 * therefore limited to the maximum size of a single packet (64K-1 bytes),
 * which is already enforced by lwIP's 16-bit length parameter to pbuf_alloc().
 *
 * The actual transmission may enforce a lower limit, though.  The full packet
 * size must not exceed the same 64K-1 limit, and that includes any headers
 * that still have to be prepended to the given packet.  The size of those
 * headers depends on the socket type (IPv4/IPv6) and the IP_HDRINCL setting.
 *
 * The default is equal to the maximum here, because if a (by definition,
 * privileged) application wishes to send large raw packets, it probably has a
 * good reason, and we do not want to get in its way.
 */
#define RAW_MAX_PAYLOAD	(UINT16_MAX)

#define RAW_SNDBUF_MIN	1		/* minimum RAW send buffer size */
#define RAW_SNDBUF_DEF	RAW_MAX_PAYLOAD	/* default RAW send buffer size */
#define RAW_SNDBUF_MAX	RAW_MAX_PAYLOAD	/* maximum RAW send buffer size */
#define RAW_RCVBUF_MIN	MEMPOOL_BUFSIZE	/* minimum RAW receive buffer size */
#define RAW_RCVBUF_DEF	32768		/* default RAW receive buffer size */
#define RAW_RCVBUF_MAX	65536		/* maximum RAW receive buffer size */

static struct rawsock {
	struct pktsock raw_pktsock;		/* packet socket object */
	struct raw_pcb *raw_pcb;		/* lwIP RAW control block */
	TAILQ_ENTRY(rawsock) raw_next;		/* next in active/free list */
	struct icmp6_filter raw_icmp6filter;	/* ICMPv6 type filter */
} raw_array[NR_RAWSOCK];

static TAILQ_HEAD(, rawsock) raw_freelist;	/* list of free RAW sockets */
static TAILQ_HEAD(, rawsock) raw_activelist;	/* list, in-use RAW sockets */

static const struct sockevent_ops rawsock_ops;

#define rawsock_get_sock(raw)	(ipsock_get_sock(rawsock_get_ipsock(raw)))
#define rawsock_get_ipsock(raw)	(pktsock_get_ipsock(&(raw)->raw_pktsock))
#define rawsock_is_ipv6(raw)	(ipsock_is_ipv6(rawsock_get_ipsock(raw)))
#define rawsock_is_v6only(raw)	(ipsock_is_v6only(rawsock_get_ipsock(raw)))
#define rawsock_is_conn(raw)	\
	(raw_flags((raw)->raw_pcb) & RAW_FLAGS_CONNECTED)
#define rawsock_is_hdrincl(raw)	\
	(raw_flags((raw)->raw_pcb) & RAW_FLAGS_HDRINCL)

static ssize_t rawsock_pcblist(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

/* The CTL_NET {PF_INET,PF_INET6} IPPROTO_RAW subtree. */
/* All dynamically numbered; the sendspace/recvspace entries are ours. */
static struct rmib_node net_inet_raw_table[] = {
	RMIB_INT(RMIB_RO, RAW_SNDBUF_DEF, "sendspace",
	    "Default RAW send buffer size"),
	RMIB_INT(RMIB_RO, RAW_RCVBUF_DEF, "recvspace",
	    "Default RAW receive buffer size"),
	RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, rawsock_pcblist, "pcblist",
	    "RAW IP protocol control block list"),
};

static struct rmib_node net_inet_raw_node =
    RMIB_NODE(RMIB_RO, net_inet_raw_table, "raw", "RAW IPv4 settings");
static struct rmib_node net_inet6_raw6_node =
    RMIB_NODE(RMIB_RO, net_inet_raw_table, "raw6", "RAW IPv6 settings");

/*
 * Initialize the raw sockets module.
 */
void
rawsock_init(void)
{
	unsigned int slot;

	/* Initialize the list of free RAW sockets. */
	TAILQ_INIT(&raw_freelist);

	for (slot = 0; slot < __arraycount(raw_array); slot++)
		TAILQ_INSERT_TAIL(&raw_freelist, &raw_array[slot], raw_next);

	/* Initialize the list of active RAW sockets. */
	TAILQ_INIT(&raw_activelist);

	/* Register the net.inet.raw and net.inet6.raw6 RMIB subtrees. */
	mibtree_register_inet(PF_INET, IPPROTO_RAW, &net_inet_raw_node);
	mibtree_register_inet(PF_INET6, IPPROTO_RAW, &net_inet6_raw6_node);
}

/*
 * Check whether the given arrived IPv6 packet is fit to be received on the
 * given raw socket.
 */
static int
rawsock_check_v6(struct rawsock * raw, struct pbuf * pbuf)
{
	uint8_t type;

	assert(rawsock_is_ipv6(raw));

	/*
	 * For ICMPv6 packets, test against the configured type filter.
	 */
	if (raw->raw_pcb->protocol == IPPROTO_ICMPV6) {
		if (pbuf->len < offsetof(struct icmp6_hdr, icmp6_dataun))
			return FALSE;

		memcpy(&type, &((struct icmp6_hdr *)pbuf->payload)->icmp6_type,
		    sizeof(type));

		if (!ICMP6_FILTER_WILLPASS((int)type, &raw->raw_icmp6filter))
			return FALSE;
	}

	/*
	 * For ICMPv6 packets, or if IPV6_CHECKSUM is enabled, we have to
	 * verify the checksum of the packet before passing it to the user.
	 * This is costly, but it needs to be done and lwIP is not doing it for
	 * us (as of writing, anyway), even though it maintains the offset..
	 */
	if (raw->raw_pcb->chksum_reqd &&
	    (pbuf->tot_len < raw->raw_pcb->chksum_offset + sizeof(uint16_t) ||
	    ip6_chksum_pseudo(pbuf, raw->raw_pcb->protocol, pbuf->tot_len,
	    ip6_current_src_addr(), ip6_current_dest_addr()) != 0)) {
		return FALSE;
	}

	/* No reason to filter out this packet. */
	return TRUE;
}

/*
 * Adjust the given arrived IPv4 packet by changing the length and offset
 * fields to host-byte order, as is done by the BSDs.  This effectively mirrors
 * the swapping part of the preparation done on IPv4 packets being sent if the
 * IP_HDRINCL socket option is enabled.
 */
static void
rawsock_adjust_v4(struct pbuf * pbuf)
{
	struct ip_hdr *iphdr;

	if (pbuf->len < sizeof(struct ip_hdr))
		return;

	iphdr = (struct ip_hdr *)pbuf->payload;

	/*
	 * W. Richard Stevens also mentions ip_id, but at least on NetBSD that
	 * field seems to be swapped neither when sending nor when receiving..
	 */
	IPH_LEN(iphdr) = htons(IPH_LEN(iphdr));
	IPH_OFFSET(iphdr) = htons(IPH_OFFSET(iphdr));
}

/*
 * A packet has arrived on a raw socket.  Since the same packet may have to be
 * delivered to multiple raw sockets, we always return 0 (= not consumed) from
 * this function.  As such, we must make a copy of the given packet if we want
 * to keep it, and never free it.
 */
static uint8_t
rawsock_input(void * arg, struct raw_pcb * pcb __unused, struct pbuf * psrc,
	const ip_addr_t * srcaddr)
{
	struct rawsock *raw = (struct rawsock *)arg;
	struct pbuf *pbuf;
	int off, hdrlen;

	assert(raw->raw_pcb == pcb);

	/*
	 * If adding this packet would cause the receive buffer to go beyond
	 * the current limit, drop the new packet.  This is just an estimation,
	 * because the copy we are about to make may not take the exact same
	 * amount of memory, due to the fact that 1) the pbuf we're given has
	 * an unknown set of headers in front of it, and 2) we need to store
	 * extra information in our copy.  The return value of this call, if
	 * not -1, is the number of bytes we need to reserve to store that
	 * extra information.
	 */
	if ((hdrlen = pktsock_test_input(&raw->raw_pktsock, psrc)) < 0)
		return 0;

	/*
	 * Raw IPv6 sockets receive only the actual packet data, whereas raw
	 * IPv4 sockets receive the IP header as well.
	 */
	if (ip_current_is_v6()) {
		off = ip_current_header_tot_len();

		util_pbuf_header(psrc, -off);

		if (!rawsock_check_v6(raw, psrc)) {
			util_pbuf_header(psrc, off);

			return 0;
		}
	} else {
		/*
		 * For IPv6 sockets, drop the packet if it was sent as an IPv4
		 * packet and checksumming is enabled (this includes ICMPv6).
		 * Otherwise, the packet would bypass the above checks that we
		 * perform on IPv6 packets.  Applications that want to use a
		 * dual-stack protocol with checksumming will have to do the
		 * checksum verification part themselves.  Presumably the two
		 * different pseudoheaders would result in different checksums
		 * anyhow, so it would be useless to try to support that.
		 *
		 * Beyond that, for IPv4 packets on IPv6 sockets, hide the IPv4
		 * header.
		 */
		if (rawsock_is_ipv6(raw)) {
			if (raw->raw_pcb->chksum_reqd)
				return 0;

			off = IP_HLEN;

			util_pbuf_header(psrc, -off);
		} else
			off = 0;
	}

	/*
	 * We need to make a copy of the incoming packet.  If we eat the one
	 * given to us, this will 1) stop any other raw sockets from getting
	 * the same packet, 2) allow a single raw socket to discard all TCP/UDP
	 * traffic, and 3) present us with a problem on how to store ancillary
	 * data.  Raw sockets are not that performance critical so the extra
	 * copy -even when not always necessary- is not that big of a deal.
	 */
	if ((pbuf = pchain_alloc(PBUF_RAW, hdrlen + psrc->tot_len)) == NULL) {
		if (off > 0)
			util_pbuf_header(psrc, off);

		return 0;
	}

	util_pbuf_header(pbuf, -hdrlen);

	if (pbuf_copy(pbuf, psrc) != ERR_OK)
		panic("unexpected pbuf copy failure");

	pbuf->flags |= psrc->flags & (PBUF_FLAG_LLMCAST | PBUF_FLAG_LLBCAST);

	if (off > 0)
		util_pbuf_header(psrc, off);

	if (!rawsock_is_ipv6(raw))
		rawsock_adjust_v4(pbuf);

	pktsock_input(&raw->raw_pktsock, pbuf, srcaddr, 0);

	return 0;
}

/*
 * Create a raw socket.
 */
sockid_t
rawsock_socket(int domain, int protocol, struct sock ** sockp,
	const struct sockevent_ops ** ops)
{
	struct rawsock *raw;
	unsigned int flags;
	uint8_t ip_type;

	if (protocol < 0 || protocol > UINT8_MAX)
		return EPROTONOSUPPORT;

	if (TAILQ_EMPTY(&raw_freelist))
		return ENOBUFS;

	raw = TAILQ_FIRST(&raw_freelist);

	/*
	 * Initialize the structure.  Do not memset it to zero, as it is still
	 * part of the linked free list.  Initialization may still fail.
	 */

	ip_type = pktsock_socket(&raw->raw_pktsock, domain, RAW_SNDBUF_DEF,
	    RAW_RCVBUF_DEF, sockp);

	/* We should have enough PCBs so this call should not fail.. */
	if ((raw->raw_pcb = raw_new_ip_type(ip_type, protocol)) == NULL)
		return ENOBUFS;
	raw_recv(raw->raw_pcb, rawsock_input, (void *)raw);

	/* By default, the multicast TTL is 1 and looping is enabled. */
	raw_set_multicast_ttl(raw->raw_pcb, 1);

	flags = raw_flags(raw->raw_pcb);
	raw_setflags(raw->raw_pcb, flags | RAW_FLAGS_MULTICAST_LOOP);

	/*
	 * For ICMPv6, checksum generation and verification is mandatory and
	 * type filtering of incoming packets is supported (RFC 3542).  For all
	 * other IPv6 protocols, checksumming may be turned on by the user.
	 */
	if (rawsock_is_ipv6(raw) && protocol == IPPROTO_ICMPV6) {
		raw->raw_pcb->chksum_reqd = 1;
		raw->raw_pcb->chksum_offset =
		    offsetof(struct icmp6_hdr, icmp6_cksum);

		ICMP6_FILTER_SETPASSALL(&raw->raw_icmp6filter);
	} else
		raw->raw_pcb->chksum_reqd = 0;

	TAILQ_REMOVE(&raw_freelist, raw, raw_next);

	TAILQ_INSERT_TAIL(&raw_activelist, raw, raw_next);

	*ops = &rawsock_ops;
	return SOCKID_RAW | (sockid_t)(raw - raw_array);
}

/*
 * Bind a raw socket to a local address.
 */
static int
rawsock_bind(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt)
{
	struct rawsock *raw = (struct rawsock *)sock;
	ip_addr_t ipaddr;
	err_t err;
	int r;

	/*
	 * Raw sockets may be rebound even if that is not too useful.  However,
	 * we do not allow (re)binding when the socket is connected, so as to
	 * eliminate any problems with source and destination type mismatches:
	 * such mismatches are detected at connect time, and rebinding would
	 * avoid those, possibly triggering lwIP asserts as a result.
	 */
	if (rawsock_is_conn(raw))
		return EINVAL;

	if ((r = ipsock_get_src_addr(rawsock_get_ipsock(raw), addr, addr_len,
	    user_endpt, &raw->raw_pcb->local_ip, 0 /*local_port*/,
	    TRUE /*allow_mcast*/, &ipaddr, NULL /*portp*/)) != OK)
		return r;

	err = raw_bind(raw->raw_pcb, &ipaddr);

	return util_convert_err(err);
}

/*
 * Connect a raw socket to a remote address.
 */
static int
rawsock_connect(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt __unused)
{
	struct rawsock *raw = (struct rawsock *)sock;
	const ip_addr_t *src_addr;
	ip_addr_t dst_addr;
	struct ifdev *ifdev;
	uint32_t ifindex, ifindex2;
	err_t err;
	int r;

	/*
	 * One may "unconnect" socket by providing an address with family
	 * AF_UNSPEC.
	 */
	if (addr_is_unspec(addr, addr_len)) {
		raw_disconnect(raw->raw_pcb);

		return OK;
	}

	if ((r = ipsock_get_dst_addr(rawsock_get_ipsock(raw), addr, addr_len,
	    &raw->raw_pcb->local_ip, &dst_addr, NULL /*dst_port*/)) != OK)
		return r;

	/*
	 * Bind explicitly to a source address if the PCB is not bound to one
	 * yet.  This is expected in the BSD socket API, but lwIP does not do
	 * it for us.
	 */
	if (ip_addr_isany(&raw->raw_pcb->local_ip)) {
		/* Help the multicast case a bit, if possible. */
		ifdev = NULL;
		if (ip_addr_ismulticast(&dst_addr)) {
			ifindex = pktsock_get_ifindex(&raw->raw_pktsock);
			ifindex2 = raw_get_multicast_netif_index(raw->raw_pcb);
			if (ifindex == 0)
				ifindex = ifindex2;

			if (ifindex != 0) {
				ifdev = ifdev_get_by_index(ifindex);

				if (ifdev == NULL)
					return ENXIO;
			}
		}

		src_addr = ifaddr_select(&dst_addr, ifdev, NULL /*ifdevp*/);

		if (src_addr == NULL)
			return EHOSTUNREACH;

		err = raw_bind(raw->raw_pcb, src_addr);

		if (err != ERR_OK)
			return util_convert_err(err);
	}

	/*
	 * Connecting a raw socket serves two main purposes: 1) the socket uses
	 * the address as destination when sending, and 2) the socket receives
	 * packets from only the connected address.
	 */
	err = raw_connect(raw->raw_pcb, &dst_addr);

	if (err != ERR_OK)
		return util_convert_err(err);

	return OK;
}

/*
 * Perform preliminary checks on a send request.
 */
static int
rawsock_pre_send(struct sock * sock, size_t len, socklen_t ctl_len __unused,
	const struct sockaddr * addr, socklen_t addr_len __unused,
	endpoint_t user_endpt __unused, int flags)
{
	struct rawsock *raw = (struct rawsock *)sock;

	if ((flags & ~MSG_DONTROUTE) != 0)
		return EOPNOTSUPP;

	if (!rawsock_is_conn(raw) && addr == NULL)
		return EDESTADDRREQ;

	/*
	 * This is only one part of the length check.  The rest is done from
	 * rawsock_send(), once we have more information.
	 */
	if (len > ipsock_get_sndbuf(rawsock_get_ipsock(raw)))
		return EMSGSIZE;

	return OK;
}

/*
 * Swap IP-level options between the RAW PCB and the packet options structure,
 * for all options that have their flag set in the packet options structure.
 * This function is called twice when sending a packet.  The result is that the
 * flagged options are overridden for only the packet being sent.
 */
static void
rawsock_swap_opt(struct rawsock * raw, struct pktopt * pkto)
{
	uint8_t tos, ttl, mcast_ttl;

	if (pkto->pkto_flags & PKTOF_TOS) {
		tos = raw->raw_pcb->tos;
		raw->raw_pcb->tos = pkto->pkto_tos;
		pkto->pkto_tos = tos;
	}

	if (pkto->pkto_flags & PKTOF_TTL) {
		ttl = raw->raw_pcb->ttl;
		mcast_ttl = raw_get_multicast_ttl(raw->raw_pcb);
		raw->raw_pcb->ttl = pkto->pkto_ttl;
		raw_set_multicast_ttl(raw->raw_pcb, pkto->pkto_ttl);
		pkto->pkto_ttl = ttl;
		pkto->pkto_mcast_ttl = mcast_ttl;
	}
}

/*
 * We are about to send the given packet that already includes an IPv4 header,
 * because the IP_HDRINCL option is enabled on a raw IPv4 socket.  Prepare the
 * IPv4 header for sending, by modifying a few fields in it, as expected by
 * userland.
 */
static int
rawsock_prepare_hdrincl(struct rawsock * raw, struct pbuf * pbuf,
	const ip_addr_t * src_addr)
{
	struct ip_hdr *iphdr;
	size_t hlen;

	/*
	 * lwIP obtains the destination address from the IP packet header in
	 * this case, so make sure the packet has a full-sized header.
	 */
	if (pbuf->len < sizeof(struct ip_hdr))
		return EINVAL;

	iphdr = (struct ip_hdr *)pbuf->payload;

	/*
	 * Fill in the source address if it is not set, and do the byte
	 * swapping and checksum computation common for the BSDs, without which
	 * ping(8) and traceroute(8) do not work properly.  We consider this a
	 * convenience feature, so malformed packets are simply sent as is.
	 * TODO: deal with type punning..
	 */
	hlen = (size_t)IPH_HL(iphdr) << 2;

	if (pbuf->len >= hlen) {
		/* Fill in the source address if it is blank. */
		if (iphdr->src.addr == PP_HTONL(INADDR_ANY)) {
			assert(IP_IS_V4(src_addr));

			iphdr->src.addr = ip_addr_get_ip4_u32(src_addr);
		}

		IPH_LEN(iphdr) = htons(IPH_LEN(iphdr));
		IPH_OFFSET(iphdr) = htons(IPH_OFFSET(iphdr));
		IPH_CHKSUM(iphdr) = 0;

		IPH_CHKSUM(iphdr) = inet_chksum(iphdr, hlen);
	}

	return OK;
}

/*
 * Send a packet on a raw socket.
 */
static int
rawsock_send(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl __unused,
	socklen_t ctl_len __unused, socklen_t * ctl_off __unused,
	const struct sockaddr * addr, socklen_t addr_len,
	endpoint_t user_endpt __unused, int flags, size_t min __unused)
{
	struct rawsock *raw = (struct rawsock *)sock;
	struct pktopt pktopt;
	struct pbuf *pbuf;
	struct ifdev *ifdev;
	struct netif *netif;
	const ip_addr_t *dst_addrp, *src_addrp;
	ip_addr_t src_addr, dst_addr; /* for storage only; not always used! */
	size_t hdrlen;
	uint32_t ifindex;
	err_t err;
	int r;

	/* Copy in and parse any packet options. */
	pktopt.pkto_flags = 0;

	if ((r = pktsock_get_ctl(&raw->raw_pktsock, ctl, ctl_len,
	    &pktopt)) != OK)
		return r;

	/*
	 * For a more in-depth explanation of what is going on here, see the
	 * udpsock module, which has largely the same code but with more
	 * elaborate comments.
	 */

	/*
	 * Start by checking whether the source address and/or the outgoing
	 * interface are overridden using sticky and/or ancillary options.
	 */
	if ((r = pktsock_get_pktinfo(&raw->raw_pktsock, &pktopt, &ifdev,
	    &src_addr)) != OK)
		return r;

	if (ifdev != NULL && !ip_addr_isany(&src_addr)) {
		/* This is guaranteed to be a proper local unicast address. */
		src_addrp = &src_addr;
	} else {
		src_addrp = &raw->raw_pcb->local_ip;

		/*
		 * If the socket is bound to a multicast address, use the
		 * unspecified ('any') address as source address instead.  A
		 * real source address will then be selected further below.
		 */
		if (ip_addr_ismulticast(src_addrp))
			src_addrp = IP46_ADDR_ANY(IP_GET_TYPE(src_addrp));
	}

	/*
	 * Determine the destination address to use.  If the socket is
	 * connected, always ignore any address provided in the send call.
	 */
	if (!rawsock_is_conn(raw)) {
		assert(addr != NULL); /* already checked in pre_send */

		if ((r = ipsock_get_dst_addr(rawsock_get_ipsock(raw), addr,
		    addr_len, src_addrp, &dst_addr, NULL /*dst_port*/)) != OK)
			return r;

		dst_addrp = &dst_addr;
	} else
		dst_addrp = &raw->raw_pcb->remote_ip;

	/*
	 * If the destination is a multicast address, select the outgoing
	 * interface based on the multicast interface index, if one is set.
	 * This must however *not* override an interface index already
	 * specified using IPV6_PKTINFO, as per RFC 3542 Sec. 6.7.
	 */
	if (ifdev == NULL && ip_addr_ismulticast(dst_addrp)) {
		ifindex = raw_get_multicast_netif_index(raw->raw_pcb);

		if (ifindex != NETIF_NO_INDEX)
			ifdev = ifdev_get_by_index(ifindex); /* (may fail) */
	}

	/*
	 * If an interface has been determined already now, the send operation
	 * will bypass routing.  In that case, we must perform our own checks
	 * on address zone violations, because those will not be made anywhere
	 * else.  Subsequent steps below will never introduce violations.
	 */
	if (ifdev != NULL && IP_IS_V6(dst_addrp)) {
		if (ifaddr_is_zone_mismatch(ip_2_ip6(dst_addrp), ifdev))
			return EHOSTUNREACH;

		if (IP_IS_V6(src_addrp) &&
		    ifaddr_is_zone_mismatch(ip_2_ip6(src_addrp), ifdev))
			return EHOSTUNREACH;
	}

	/*
	 * If we do not yet have an interface at this point, perform a route
	 * lookup to determine the outgoing interface, unless MSG_DONTROUTE is
	 * set.
	 */
	if (ifdev == NULL) {
		if (!(flags & MSG_DONTROUTE)) {
			/*
			 * ip_route() should never be called with an
			 * IPADDR_TYPE_ANY type address.  This is a lwIP-
			 * internal requirement; while we override both routing
			 * functions, we do not deviate from it.
			 */
			if (IP_IS_ANY_TYPE_VAL(*src_addrp))
				src_addrp =
				    IP46_ADDR_ANY(IP_GET_TYPE(dst_addrp));

			/* Perform the route lookup. */
			if ((netif = ip_route(src_addrp, dst_addrp)) == NULL)
				return EHOSTUNREACH;

			ifdev = netif_get_ifdev(netif);
		} else {
			if ((ifdev = ifaddr_map_by_subnet(dst_addrp)) == NULL)
				return EHOSTUNREACH;
		}
	}

	/*
	 * At this point we have an outgoing interface.  If we do not have a
	 * source address yet, pick one now.  As a sidenote, if the destination
	 * address is scoped but has no zone, we could also fill in the zone
	 * now.  We let lwIP handle that instead, though.
	 */
	assert(ifdev != NULL);

	if (ip_addr_isany(src_addrp)) {
		src_addrp = ifaddr_select(dst_addrp, ifdev, NULL /*ifdevp*/);

		if (src_addrp == NULL)
			return EHOSTUNREACH;
	}

	/*
	 * Now that we know the full conditions of what we are about to send,
	 * check whether the packet size leaves enough room for lwIP to prepend
	 * headers.  If so, allocate a chain of pbufs for the packet.
	 */
	assert(len <= RAW_MAX_PAYLOAD);

	if (rawsock_is_hdrincl(raw))
		hdrlen = 0;
	else if (IP_IS_V6(dst_addrp))
		hdrlen = IP6_HLEN;
	else
		hdrlen = IP_HLEN;

	if (hdrlen + len > RAW_MAX_PAYLOAD)
		return EMSGSIZE;

	if ((pbuf = pchain_alloc(PBUF_IP, len)) == NULL)
		return ENOBUFS;

	/* Copy in the packet data. */
	if ((r = pktsock_get_data(&raw->raw_pktsock, data, len, pbuf)) != OK) {
		pbuf_free(pbuf);

		return r;
	}

	/*
	 * If the user has turned on IPV6_CHECKSUM, ensure that the packet is
	 * not only large enough to have the checksum stored at the configured
	 * place, but also that the checksum fits within the first pbuf: if we
	 * do not test this here, an assert will trigger in lwIP later.  Also
	 * zero out the checksum field first, because lwIP does not do that.
	 */
	if (raw->raw_pcb->chksum_reqd) {
		if (pbuf->len < raw->raw_pcb->chksum_offset +
		    sizeof(uint16_t)) {
			pbuf_free(pbuf);

			return EINVAL;
		}

		memset((char *)pbuf->payload + raw->raw_pcb->chksum_offset, 0,
		    sizeof(uint16_t));
	}

	/*
	 * For sockets where an IPv4 header is already included in the packet,
	 * we need to alter a few header fields to be compatible with BSD.
	 */
	if (rawsock_is_hdrincl(raw) &&
	    (r = rawsock_prepare_hdrincl(raw, pbuf, src_addrp)) != OK) {
		pbuf_free(pbuf);

		return r;
	}

	/* Set broadcast/multicast flags for accounting purposes. */
	if (ip_addr_ismulticast(dst_addrp))
		pbuf->flags |= PBUF_FLAG_LLMCAST;
	else if (ip_addr_isbroadcast(dst_addrp, ifdev_get_netif(ifdev)))
		pbuf->flags |= PBUF_FLAG_LLBCAST;

	/* Send the packet. */
	rawsock_swap_opt(raw, &pktopt);

	assert(!ip_addr_isany(src_addrp));
	assert(!ip_addr_ismulticast(src_addrp));

	err = raw_sendto_if_src(raw->raw_pcb, pbuf, dst_addrp,
	    ifdev_get_netif(ifdev), src_addrp);

	rawsock_swap_opt(raw, &pktopt);

	/* Free the pbuf again. */
	pbuf_free(pbuf);

	/*
	 * On success, make sure to return the size of the sent packet as well.
	 * As an aside: ctl_off need not be updated, as it is not returned.
	 */
	if ((r = util_convert_err(err)) == OK)
		*off = len;
	return r;
}

/*
 * Update the set of flag-type socket options on a raw socket.
 */
static void
rawsock_setsockmask(struct sock * sock, unsigned int mask)
{
	struct rawsock *raw = (struct rawsock *)sock;

	/*
	 * FIXME: raw sockets are not supposed to have a broardcast check, so
	 * perhaps just remove this and instead always set SOF_BROADCAST?
	 */
	if (mask & SO_BROADCAST)
		ip_set_option(raw->raw_pcb, SOF_BROADCAST);
	else
		ip_reset_option(raw->raw_pcb, SOF_BROADCAST);
}

/*
 * Prepare a helper structure for IP-level option processing.
 */
static void
rawsock_get_ipopts(struct rawsock * raw, struct ipopts * ipopts)
{

	ipopts->local_ip = &raw->raw_pcb->local_ip;
	ipopts->remote_ip = &raw->raw_pcb->remote_ip;
	ipopts->tos = &raw->raw_pcb->tos;
	ipopts->ttl = &raw->raw_pcb->ttl;
	ipopts->sndmin = RAW_SNDBUF_MIN;
	ipopts->sndmax = RAW_SNDBUF_MAX;
	ipopts->rcvmin = RAW_RCVBUF_MIN;
	ipopts->rcvmax = RAW_RCVBUF_MAX;
}

/*
 * Set socket options on a raw socket.
 */
static int
rawsock_setsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t len)
{
	struct rawsock *raw = (struct rawsock *)sock;
	struct ipopts ipopts;
	struct icmp6_filter filter;
	ip_addr_t ipaddr;
	struct in_addr in_addr;
	struct ifdev *ifdev;
	unsigned int flags;
	uint32_t ifindex;
	uint8_t byte;
	int r, val;

	/*
	 * Unfortunately, we have to duplicate most of the multicast options
	 * rather than sharing them with udpsock at the pktsock level.  The
	 * reason is that each of the PCBs have their own multicast abstraction
	 * functions and so we cannot merge the rest.  Same for getsockopt.
	 */

	switch (level) {
	case IPPROTO_IP:
		if (rawsock_is_ipv6(raw))
			break;

		switch (name) {
		case IP_HDRINCL:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val) {
				raw_setflags(raw->raw_pcb,
				    raw_flags(raw->raw_pcb) |
				    RAW_FLAGS_HDRINCL);
			} else {
				raw_setflags(raw->raw_pcb,
				    raw_flags(raw->raw_pcb) &
				    ~RAW_FLAGS_HDRINCL);
			}

			return OK;

		case IP_MULTICAST_IF:
			pktsock_set_mcaware(&raw->raw_pktsock);

			if ((r = sockdriver_copyin_opt(data, &in_addr,
			    sizeof(in_addr), len)) != OK)
				return r;

			ip_addr_set_ip4_u32(&ipaddr, in_addr.s_addr);

			if ((ifdev = ifaddr_map_by_addr(&ipaddr)) == NULL)
				return EADDRNOTAVAIL;

			raw_set_multicast_netif_index(raw->raw_pcb,
			    ifdev_get_index(ifdev));

			return OK;

		case IP_MULTICAST_LOOP:
			pktsock_set_mcaware(&raw->raw_pktsock);

			if ((r = sockdriver_copyin_opt(data, &byte,
			    sizeof(byte), len)) != OK)
				return r;

			flags = raw_flags(raw->raw_pcb);

			if (byte)
				flags |= RAW_FLAGS_MULTICAST_LOOP;
			else
				flags &= ~RAW_FLAGS_MULTICAST_LOOP;

			raw_setflags(raw->raw_pcb, flags);

			return OK;

		case IP_MULTICAST_TTL:
			pktsock_set_mcaware(&raw->raw_pktsock);

			if ((r = sockdriver_copyin_opt(data, &byte,
			    sizeof(byte), len)) != OK)
				return r;

			raw_set_multicast_ttl(raw->raw_pcb, byte);

			return OK;
		}

		break;

	case IPPROTO_IPV6:
		if (!rawsock_is_ipv6(raw))
			break;

		switch (name) {
		case IPV6_CHECKSUM:
			/* ICMPv6 checksums are always computed. */
			if (raw->raw_pcb->protocol == IPPROTO_ICMPV6)
				return EINVAL;

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val == -1) {
				raw->raw_pcb->chksum_reqd = 0;

				return OK;
			} else if (val >= 0 && !(val & 1)) {
				raw->raw_pcb->chksum_reqd = 1;
				raw->raw_pcb->chksum_offset = val;

				return OK;
			} else
				return EINVAL;

		case IPV6_MULTICAST_IF:
			pktsock_set_mcaware(&raw->raw_pktsock);

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val != 0) {
				ifindex = (uint32_t)val;

				ifdev = ifdev_get_by_index(ifindex);

				if (ifdev == NULL)
					return ENXIO;
			} else
				ifindex = NETIF_NO_INDEX;

			raw_set_multicast_netif_index(raw->raw_pcb, ifindex);

			return OK;

		case IPV6_MULTICAST_LOOP:
			pktsock_set_mcaware(&raw->raw_pktsock);

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < 0 || val > 1)
				return EINVAL;

			flags = raw_flags(raw->raw_pcb);

			if (val)
				flags |= RAW_FLAGS_MULTICAST_LOOP;
			else
				flags &= ~RAW_FLAGS_MULTICAST_LOOP;

			/*
			 * lwIP's IPv6 functionality does not actually check
			 * this flag at all yet.  We set it in the hope that
			 * one day this will magically start working.
			 */
			raw_setflags(raw->raw_pcb, flags);

			return OK;

		case IPV6_MULTICAST_HOPS:
			pktsock_set_mcaware(&raw->raw_pktsock);

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < -1 || val > UINT8_MAX)
				return EINVAL;

			if (val == -1)
				val = 1;

			raw_set_multicast_ttl(raw->raw_pcb, val);

			return OK;
		}

		break;

	case IPPROTO_ICMPV6:
		if (!rawsock_is_ipv6(raw) ||
		    raw->raw_pcb->protocol != IPPROTO_ICMPV6)
			break;

		switch (name) {
		case ICMP6_FILTER:
			/* Who comes up with these stupid exceptions? */
			if (len == 0) {
				ICMP6_FILTER_SETPASSALL(&raw->raw_icmp6filter);

				return OK;
			}

			if ((r = sockdriver_copyin_opt(data, &filter,
			    sizeof(filter), len)) != OK)
				return r;

			/*
			 * As always, never copy in the data into the actual
			 * destination, as any copy may run into a copy fault
			 * halfway through, potentially leaving the destination
			 * in a half-updated and thus corrupted state.
			 */
			memcpy(&raw->raw_icmp6filter, &filter, sizeof(filter));

			return OK;
		}
	}

	rawsock_get_ipopts(raw, &ipopts);

	return pktsock_setsockopt(&raw->raw_pktsock, level, name, data, len,
	    &ipopts);
}

/*
 * Retrieve socket options on a raw socket.
 */
static int
rawsock_getsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t * len)
{
	struct rawsock *raw = (struct rawsock *)sock;
	struct ipopts ipopts;
	const ip4_addr_t *ip4addr;
	struct in_addr in_addr;
	struct ifdev *ifdev;
	unsigned int flags;
	uint32_t ifindex;
	uint8_t byte;
	int val;

	switch (level) {
	case IPPROTO_IP:
		if (rawsock_is_ipv6(raw))
			break;

		switch (name) {
		case IP_HDRINCL:
			val = !!rawsock_is_hdrincl(raw);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IP_MULTICAST_IF:
			ifindex = raw_get_multicast_netif_index(raw->raw_pcb);

			/*
			 * Map back from the interface index to the IPv4
			 * address assigned to the corresponding interface.
			 * Should this not work out, return the 'any' address.
			 */
			if (ifindex != NETIF_NO_INDEX &&
			   (ifdev = ifdev_get_by_index(ifindex)) != NULL) {
				ip4addr =
				    netif_ip4_addr(ifdev_get_netif(ifdev));

				in_addr.s_addr = ip4_addr_get_u32(ip4addr);
			} else
				in_addr.s_addr = PP_HTONL(INADDR_ANY);

			return sockdriver_copyout_opt(data, &in_addr,
			    sizeof(in_addr), len);

		case IP_MULTICAST_LOOP:
			flags = raw_flags(raw->raw_pcb);

			byte = !!(flags & RAW_FLAGS_MULTICAST_LOOP);

			return sockdriver_copyout_opt(data, &byte,
			    sizeof(byte), len);

		case IP_MULTICAST_TTL:
			byte = raw_get_multicast_ttl(raw->raw_pcb);

			return sockdriver_copyout_opt(data, &byte,
			    sizeof(byte), len);
		}

		break;

	case IPPROTO_IPV6:
		if (!rawsock_is_ipv6(raw))
			break;

		switch (name) {
		case IPV6_CHECKSUM:
			if (raw->raw_pcb->chksum_reqd)
				val = raw->raw_pcb->chksum_offset;
			else
				val = -1;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_MULTICAST_IF:
			ifindex = raw_get_multicast_netif_index(raw->raw_pcb);

			val = (int)ifindex;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_MULTICAST_LOOP:
			flags = raw_flags(raw->raw_pcb);

			val = !!(flags & RAW_FLAGS_MULTICAST_LOOP);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_MULTICAST_HOPS:
			val = raw_get_multicast_ttl(raw->raw_pcb);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;

	case IPPROTO_ICMPV6:
		if (!rawsock_is_ipv6(raw) ||
		    raw->raw_pcb->protocol != IPPROTO_ICMPV6)
			break;

		switch (name) {
		case ICMP6_FILTER:
			return sockdriver_copyout_opt(data,
			    &raw->raw_icmp6filter,
			    sizeof(raw->raw_icmp6filter), len);
		}

		break;
	}

	rawsock_get_ipopts(raw, &ipopts);

	return pktsock_getsockopt(&raw->raw_pktsock, level, name, data, len,
	    &ipopts);
}

/*
 * Retrieve the local socket address of a raw socket.
 */
static int
rawsock_getsockname(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct rawsock *raw = (struct rawsock *)sock;

	ipsock_put_addr(rawsock_get_ipsock(raw), addr, addr_len,
	    &raw->raw_pcb->local_ip, 0 /*port*/);

	return OK;
}

/*
 * Retrieve the remote socket address of a raw socket.
 */
static int
rawsock_getpeername(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct rawsock *raw = (struct rawsock *)sock;

	if (!rawsock_is_conn(raw))
		return ENOTCONN;

	ipsock_put_addr(rawsock_get_ipsock(raw), addr, addr_len,
	    &raw->raw_pcb->remote_ip, 0 /*port*/);

	return OK;
}

/*
 * Shut down a raw socket for reading and/or writing.
 */
static int
rawsock_shutdown(struct sock * sock, unsigned int mask)
{
	struct rawsock *raw = (struct rawsock *)sock;

	if (mask & SFL_SHUT_RD)
		raw_recv(raw->raw_pcb, NULL, NULL);

	pktsock_shutdown(&raw->raw_pktsock, mask);

	return OK;
}

/*
 * Close a raw socket.
 */
static int
rawsock_close(struct sock * sock, int force __unused)
{
	struct rawsock *raw = (struct rawsock *)sock;

	raw_recv(raw->raw_pcb, NULL, NULL);

	raw_remove(raw->raw_pcb);
	raw->raw_pcb = NULL;

	pktsock_close(&raw->raw_pktsock);

	return OK;
}

/*
 * Free up a closed raw socket.
 */
static void
rawsock_free(struct sock * sock)
{
	struct rawsock *raw = (struct rawsock *)sock;

	assert(raw->raw_pcb == NULL);

	TAILQ_REMOVE(&raw_activelist, raw, raw_next);

	TAILQ_INSERT_HEAD(&raw_freelist, raw, raw_next);
}

/*
 * Fill the given kinfo_pcb sysctl(7) structure with information about the RAW
 * PCB identified by the given pointer.
 */
static void
rawsock_get_info(struct kinfo_pcb * ki, const void * ptr)
{
	const struct raw_pcb *pcb = (const struct raw_pcb *)ptr;
	struct rawsock *raw;

	/* We iterate our own list so we can't find "strange" PCBs. */
	raw = (struct rawsock *)pcb->recv_arg;
	assert(raw >= raw_array &&
	    raw < &raw_array[__arraycount(raw_array)]);

	ki->ki_type = SOCK_RAW;
	ki->ki_protocol = pcb->protocol;

	ipsock_get_info(ki, &pcb->local_ip, 0 /*local_port*/,
	    &raw->raw_pcb->remote_ip, 0 /*remote_port*/);

	/* TODO: change this so that sockstat(1) may work one day. */
	ki->ki_sockaddr = (uint64_t)(uintptr_t)rawsock_get_sock(raw);

	ki->ki_rcvq = pktsock_get_recvlen(&raw->raw_pktsock);

	if (rawsock_is_hdrincl(raw))
		ki->ki_pflags |= INP_HDRINCL;
}

/*
 * Given either NULL or a previously returned RAW PCB pointer, return the first
 * or next RAW PCB pointer, or NULL if there are no more.  lwIP does not expose
 * 'raw_pcbs', but other modules in this service may also use RAW PCBs (which
 * should then stay hidden), so we iterate through our own list instead.
 */
static const void *
rawsock_enum(const void * last)
{
	const struct raw_pcb *pcb;
	struct rawsock *raw;

	if (last != NULL) {
		pcb = (const struct raw_pcb *)last;

		raw = (struct rawsock *)pcb->recv_arg;
		assert(raw >= raw_array &&
		    raw < &raw_array[__arraycount(raw_array)]);

		raw = TAILQ_NEXT(raw, raw_next);
	} else
		raw = TAILQ_FIRST(&raw_activelist);

	if (raw != NULL)
		return raw->raw_pcb;
	else
		return NULL;
}

/*
 * Obtain the list of RAW protocol control blocks, for sysctl(7).
 */
static ssize_t
rawsock_pcblist(struct rmib_call * call, struct rmib_node * node,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{

	return util_pcblist(call, oldp, rawsock_enum, rawsock_get_info);
}

static const struct sockevent_ops rawsock_ops = {
	.sop_bind		= rawsock_bind,
	.sop_connect		= rawsock_connect,
	.sop_pre_send		= rawsock_pre_send,
	.sop_send		= rawsock_send,
	.sop_pre_recv		= pktsock_pre_recv,
	.sop_recv		= pktsock_recv,
	.sop_test_recv		= pktsock_test_recv,
	.sop_ioctl		= ifconf_ioctl,
	.sop_setsockmask	= rawsock_setsockmask,
	.sop_setsockopt		= rawsock_setsockopt,
	.sop_getsockopt		= rawsock_getsockopt,
	.sop_getsockname	= rawsock_getsockname,
	.sop_getpeername	= rawsock_getpeername,
	.sop_shutdown		= rawsock_shutdown,
	.sop_close		= rawsock_close,
	.sop_free		= rawsock_free
};
