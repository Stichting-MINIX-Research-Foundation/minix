/* LWIP service - udpsock.c - UDP sockets */

#include "lwip.h"
#include "ifaddr.h"
#include "pktsock.h"

#include "lwip/udp.h"

#include <netinet/udp.h>
#include <netinet/ip_var.h>
#include <netinet/udp_var.h>

/* The number of UDP sockets.  Inherited from the lwIP configuration. */
#define NR_UDPSOCK	MEMP_NUM_UDP_PCB

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
 */
#define UDP_MAX_PAYLOAD	(UINT16_MAX)

#define UDP_SNDBUF_MIN	1		/* minimum UDP send buffer size */
#define UDP_SNDBUF_DEF	8192		/* default UDP send buffer size */
#define UDP_SNDBUF_MAX	UDP_MAX_PAYLOAD	/* maximum UDP send buffer size */
#define UDP_RCVBUF_MIN	MEMPOOL_BUFSIZE	/* minimum UDP receive buffer size */
#define UDP_RCVBUF_DEF	32768		/* default UDP receive buffer size */
#define UDP_RCVBUF_MAX	65536		/* maximum UDP receive buffer size */

static struct udpsock {
	struct pktsock udp_pktsock;		/* pkt socket, MUST be first */
	struct udp_pcb *udp_pcb;		/* lwIP UDP control block */
	SIMPLEQ_ENTRY(udpsock) udp_next;	/* next in free list */
} udp_array[NR_UDPSOCK];

static SIMPLEQ_HEAD(, udpsock) udp_freelist;	/* list of free UDP sockets */

static const struct sockevent_ops udpsock_ops;

#define udpsock_get_sock(udp)	(ipsock_get_sock(udpsock_get_ipsock(udp)))
#define udpsock_get_ipsock(udp)	(pktsock_get_ipsock(&(udp)->udp_pktsock))
#define udpsock_is_ipv6(udp)	(ipsock_is_ipv6(udpsock_get_ipsock(udp)))
#define udpsock_is_conn(udp)	\
	(udp_flags((udp)->udp_pcb) & UDP_FLAGS_CONNECTED)

static ssize_t udpsock_pcblist(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

/* The CTL_NET {PF_INET,PF_INET6} IPPROTO_UDP subtree. */
/* TODO: add many more and make some of them writable.. */
static struct rmib_node net_inet_udp_table[] = {
/* 1*/	[UDPCTL_CHECKSUM]	= RMIB_INT(RMIB_RO, 1, "checksum",
				    "Compute UDP checksums"),
/* 2*/	[UDPCTL_SENDSPACE]	= RMIB_INT(RMIB_RO, UDP_SNDBUF_DEF,
				    "sendspace",
				    "Default UDP send buffer size"),
/* 3*/	[UDPCTL_RECVSPACE]	= RMIB_INT(RMIB_RO, UDP_RCVBUF_DEF,
				    "recvspace",
				    "Default UDP receive buffer size"),
/* 4*/	[UDPCTL_LOOPBACKCKSUM]	= RMIB_FUNC(RMIB_RW | CTLTYPE_INT, sizeof(int),
				    loopif_cksum, "do_loopback_cksum",
				    "Perform UDP checksum on loopback"),
/*+0*/	[UDPCTL_MAXID]		= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0,
				    udpsock_pcblist, "pcblist",
				    "UDP protocol control block list"),
};

static struct rmib_node net_inet_udp_node =
    RMIB_NODE(RMIB_RO, net_inet_udp_table, "udp", "UDPv4 related settings");
static struct rmib_node net_inet6_udp6_node =
    RMIB_NODE(RMIB_RO, net_inet_udp_table, "udp6", "UDPv6 related settings");

/*
 * Initialize the UDP sockets module.
 */
void
udpsock_init(void)
{
	unsigned int slot;

	/* Initialize the list of free UDP sockets. */
	SIMPLEQ_INIT(&udp_freelist);

	for (slot = 0; slot < __arraycount(udp_array); slot++)
		SIMPLEQ_INSERT_TAIL(&udp_freelist, &udp_array[slot], udp_next);

	/* Register the net.inet.udp and net.inet6.udp6 RMIB subtrees. */
	mibtree_register_inet(PF_INET, IPPROTO_UDP, &net_inet_udp_node);
	mibtree_register_inet(PF_INET6, IPPROTO_UDP, &net_inet6_udp6_node);
}

/*
 * A packet has arrived on a UDP socket.  We own the given packet buffer, and
 * so we must free it if we do not want to keep it.
 */
static void
udpsock_input(void * arg, struct udp_pcb * pcb __unused, struct pbuf * pbuf,
	const ip_addr_t * ipaddr, uint16_t port)
{
	struct udpsock *udp = (struct udpsock *)arg;

	/* All UDP input processing is handled by pktsock. */
	pktsock_input(&udp->udp_pktsock, pbuf, ipaddr, port);
}

/*
 * Create a UDP socket.
 */
sockid_t
udpsock_socket(int domain, int protocol, struct sock ** sockp,
	const struct sockevent_ops ** ops)
{
	struct udpsock *udp;
	unsigned int flags;
	uint8_t ip_type;

	switch (protocol) {
	case 0:
	case IPPROTO_UDP:
		break;

	/* NetBSD does not support IPPROTO_UDPLITE, even though lwIP does. */
	default:
		return EPROTONOSUPPORT;
	}

	if (SIMPLEQ_EMPTY(&udp_freelist))
		return ENOBUFS;

	udp = SIMPLEQ_FIRST(&udp_freelist);

	ip_type = pktsock_socket(&udp->udp_pktsock, domain, UDP_SNDBUF_DEF,
	    UDP_RCVBUF_DEF, sockp);

	/* We should have enough PCBs so this call should not fail.. */
	if ((udp->udp_pcb = udp_new_ip_type(ip_type)) == NULL)
		return ENOBUFS;
	udp_recv(udp->udp_pcb, udpsock_input, (void *)udp);

	/* By default, the multicast TTL is 1 and looping is enabled. */
	udp_set_multicast_ttl(udp->udp_pcb, 1);

	flags = udp_flags(udp->udp_pcb);
	udp_setflags(udp->udp_pcb, flags | UDP_FLAGS_MULTICAST_LOOP);

	SIMPLEQ_REMOVE_HEAD(&udp_freelist, udp_next);

	*ops = &udpsock_ops;
	return SOCKID_UDP | (sockid_t)(udp - udp_array);
}

/*
 * Bind a UDP socket to a local address.
 */
static int
udpsock_bind(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt)
{
	struct udpsock *udp = (struct udpsock *)sock;
	ip_addr_t ipaddr;
	uint16_t port;
	err_t err;
	int r;

	if ((r = ipsock_get_src_addr(udpsock_get_ipsock(udp), addr, addr_len,
	    user_endpt, &udp->udp_pcb->local_ip, udp->udp_pcb->local_port,
	    TRUE /*allow_mcast*/, &ipaddr, &port)) != OK)
		return r;

	err = udp_bind(udp->udp_pcb, &ipaddr, port);

	return util_convert_err(err);
}

/*
 * Connect a UDP socket to a remote address.
 */
static int
udpsock_connect(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt __unused)
{
	struct udpsock *udp = (struct udpsock *)sock;
	struct ifdev *ifdev;
	const ip_addr_t *src_addr;
	ip_addr_t dst_addr;
	uint16_t dst_port;
	uint32_t ifindex, ifindex2;
	err_t err;
	int r;

	/*
	 * One may "unconnect" socket by providing an address with family
	 * AF_UNSPEC.  Providing an <any>:0 address does not achieve the same.
	 */
	if (addr_is_unspec(addr, addr_len)) {
		udp_disconnect(udp->udp_pcb);

		return OK;
	}

	if ((r = ipsock_get_dst_addr(udpsock_get_ipsock(udp), addr,
	    addr_len, &udp->udp_pcb->local_ip, &dst_addr, &dst_port)) != OK)
		return r;

	/*
	 * Bind explicitly to a source address if the PCB is not bound to one
	 * yet.  This is expected in the BSD socket API, but lwIP does not do
	 * it for us.
	 */
	if (ip_addr_isany(&udp->udp_pcb->local_ip)) {
		/* Help the multicast case a bit, if possible. */
		ifdev = NULL;

		if (ip_addr_ismulticast(&dst_addr)) {
			ifindex = pktsock_get_ifindex(&udp->udp_pktsock);
			ifindex2 = udp_get_multicast_netif_index(udp->udp_pcb);
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

		err = udp_bind(udp->udp_pcb, src_addr,
		    udp->udp_pcb->local_port);

		if (err != ERR_OK)
			return util_convert_err(err);
	}

	/*
	 * Connecting a UDP socket serves two main purposes: 1) the socket uses
	 * the address as destination when sending, and 2) the socket receives
	 * packets from only the connected address.
	 */
	err = udp_connect(udp->udp_pcb, &dst_addr, dst_port);

	if (err != ERR_OK)
		return util_convert_err(err);

	return OK;
}

/*
 * Perform preliminary checks on a send request.
 */
static int
udpsock_pre_send(struct sock * sock, size_t len, socklen_t ctl_len __unused,
	const struct sockaddr * addr, socklen_t addr_len __unused,
	endpoint_t user_endpt __unused, int flags)
{
	struct udpsock *udp = (struct udpsock *)sock;

	if ((flags & ~MSG_DONTROUTE) != 0)
		return EOPNOTSUPP;

	if (!udpsock_is_conn(udp) && addr == NULL)
		return EDESTADDRREQ;

	/*
	 * This is only one part of the length check.  The rest is done from
	 * udpsock_send(), once we have more information.
	 */
	if (len > ipsock_get_sndbuf(udpsock_get_ipsock(udp)))
		return EMSGSIZE;

	return OK;
}

/*
 * Swap IP-level options between the UDP PCB and the packet options structure,
 * for all options that have their flag set in the packet options structure.
 * This function is called twice when sending a packet.  The result is that the
 * flagged options are overridden for only the packet being sent.
 */
static void
udpsock_swap_opt(struct udpsock * udp, struct pktopt * pkto)
{
	uint8_t tos, ttl, mcast_ttl;

	if (pkto->pkto_flags & PKTOF_TOS) {
		tos = udp->udp_pcb->tos;
		udp->udp_pcb->tos = pkto->pkto_tos;
		pkto->pkto_tos = tos;
	}

	if (pkto->pkto_flags & PKTOF_TTL) {
		ttl = udp->udp_pcb->ttl;
		mcast_ttl = udp_get_multicast_ttl(udp->udp_pcb);
		udp->udp_pcb->ttl = pkto->pkto_ttl;
		udp_set_multicast_ttl(udp->udp_pcb, pkto->pkto_mcast_ttl);
		pkto->pkto_ttl = ttl;
		pkto->pkto_mcast_ttl = mcast_ttl;
	}
}

/*
 * Send a packet on a UDP socket.
 */
static int
udpsock_send(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t * ctl_off __unused,
	const struct sockaddr * addr, socklen_t addr_len,
	endpoint_t user_endpt __unused, int flags, size_t min __unused)
{
	struct udpsock *udp = (struct udpsock *)sock;
	struct pktopt pktopt;
	struct pbuf *pbuf;
	struct ifdev *ifdev;
	struct netif *netif;
	const ip_addr_t *src_addrp, *dst_addrp;
	ip_addr_t src_addr, dst_addr; /* for storage only; not always used! */
	uint16_t dst_port;
	uint32_t ifindex;
	size_t hdrlen;
	err_t err;
	int r;

	/* Copy in and parse any packet options. */
	pktopt.pkto_flags = 0;

	if ((r = pktsock_get_ctl(&udp->udp_pktsock, ctl, ctl_len,
	    &pktopt)) != OK)
		return r;

	/*
	 * The code below will both determine an outgoing interface and a
	 * source address for the packet.  Even though lwIP could do this for
	 * us in some cases, there are other cases where we must do so
	 * ourselves, with as main reasons 1) the possibility that either or
	 * both have been provided through IPV6_PKTINFO, and 2) our intent to
	 * detect and stop zone violations for (combinations of) scoped IPv6
	 * addresses.  As a result, it is easier to simply take over the
	 * selection tasks lwIP in their entirety.
	 *
	 * Much of the same applies to rawsock_send() as well.  Functional
	 * differences (e.g. IP_HDRINCL support) as well as the PCB accesses in
	 * the code make it hard to merge the two into a single pktsock copy.
	 * Please do keep the two in sync as much as possible.
	 */

	/*
	 * Start by checking whether the source address and/or the outgoing
	 * interface are overridden using sticky and/or ancillary options.  The
	 * call to pktsock_get_pktinfo(), if successful, will either set
	 * 'ifdev' to NULL, in which case there is no override, or it will set
	 * 'ifdev' to the outgoing interface to use, and (only) in that case
	 * also fill 'src_addr', with an address that may either be a locally
	 * owned unicast address or the unspecified ('any') address.  If it is
	 * a unicast address, that is the source address to use for the packet.
	 * Otherwise, fall back to the address to which the socket is bound,
	 * which may also be the unspecified address or even a multicast
	 * address.  In those case we will pick a source address further below.
	 */
	if ((r = pktsock_get_pktinfo(&udp->udp_pktsock, &pktopt, &ifdev,
	    &src_addr)) != OK)
		return r;

	if (ifdev != NULL && !ip_addr_isany(&src_addr)) {
		/* This is guaranteed to be a proper local unicast address. */
		src_addrp = &src_addr;
	} else {
		src_addrp = &udp->udp_pcb->local_ip;

		/*
		 * If the socket is bound to a multicast address, use the
		 * unspecified ('any') address as source address instead, until
		 * we select a real source address (further below).  This
		 * substitution keeps the rest of the code a bit simpler.
		 */
		if (ip_addr_ismulticast(src_addrp))
			src_addrp = IP46_ADDR_ANY(IP_GET_TYPE(src_addrp));
	}

	/*
	 * Determine the destination address to use.  If the socket is
	 * connected, always ignore any address provided in the send call.
	 */
	if (!udpsock_is_conn(udp)) {
		assert(addr != NULL); /* already checked in pre_send */

		if ((r = ipsock_get_dst_addr(udpsock_get_ipsock(udp), addr,
		    addr_len, src_addrp, &dst_addr, &dst_port)) != OK)
			return r;

		dst_addrp = &dst_addr;
	} else {
		dst_addrp = &udp->udp_pcb->remote_ip;
		dst_port = udp->udp_pcb->remote_port;
	}

	/*
	 * If the destination is a multicast address, select the outgoing
	 * interface based on the multicast interface index, if one is set.
	 * This must be done here in order to allow the code further below to
	 * detect zone violations, because if we leave this selection to lwIP,
	 * it will not perform zone violation detection at all.  Also note that
	 * this case must *not* override an interface index already specified
	 * using IPV6_PKTINFO, as per RFC 3542 Sec. 6.7.
	 */
	if (ifdev == NULL && ip_addr_ismulticast(dst_addrp)) {
		ifindex = udp_get_multicast_netif_index(udp->udp_pcb);

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
	 * lookup to determine the outgoing interface.  Unless MSG_DONTROUTE is
	 * set (which covers SO_DONTROUTE as well), in which case we look for a
	 * local subnet that matches the destination address.
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
	 * source address yet, pick one now.
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
	assert(len <= UDP_MAX_PAYLOAD);

	if (IP_IS_V6(dst_addrp))
		hdrlen = IP6_HLEN + UDP_HLEN;
	else
		hdrlen = IP_HLEN + UDP_HLEN;

	if (hdrlen + len > UDP_MAX_PAYLOAD)
		return EMSGSIZE;

	if ((pbuf = pchain_alloc(PBUF_TRANSPORT, len)) == NULL)
		return ENOBUFS;

	/* Copy in the packet data. */
	if ((r = pktsock_get_data(&udp->udp_pktsock, data, len, pbuf)) != OK) {
		pbuf_free(pbuf);

		return r;
	}

	/*
	 * Set broadcast/multicast flags for accounting purposes.  Only the
	 * multicast flag is used for output accounting, but for loopback
	 * traffic, both flags are copied and used for input accounting and
	 * setting MSG_MCAST/MSG_BCAST.
	 */
	if (ip_addr_ismulticast(dst_addrp))
		pbuf->flags |= PBUF_FLAG_LLMCAST;
	else if (ip_addr_isbroadcast(dst_addrp, ifdev_get_netif(ifdev)))
		pbuf->flags |= PBUF_FLAG_LLBCAST;

	/* Send the packet. */
	udpsock_swap_opt(udp, &pktopt);

	assert(!ip_addr_isany(src_addrp));
	assert(!ip_addr_ismulticast(src_addrp));

	err = udp_sendto_if_src(udp->udp_pcb, pbuf, dst_addrp, dst_port,
	    ifdev_get_netif(ifdev), src_addrp);

	udpsock_swap_opt(udp, &pktopt);

	/* Free the pbuf, as a copy has been made. */
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
 * Update the set of flag-type socket options on a UDP socket.
 */
static void
udpsock_setsockmask(struct sock * sock, unsigned int mask)
{
	struct udpsock *udp = (struct udpsock *)sock;

	if (mask & SO_REUSEADDR)
		ip_set_option(udp->udp_pcb, SOF_REUSEADDR);
	else
		ip_reset_option(udp->udp_pcb, SOF_REUSEADDR);

	if (mask & SO_BROADCAST)
		ip_set_option(udp->udp_pcb, SOF_BROADCAST);
	else
		ip_reset_option(udp->udp_pcb, SOF_BROADCAST);
}

/*
 * Prepare a helper structure for IP-level option processing.
 */
static void
udpsock_get_ipopts(struct udpsock * udp, struct ipopts * ipopts)
{

	ipopts->local_ip = &udp->udp_pcb->local_ip;
	ipopts->remote_ip = &udp->udp_pcb->remote_ip;
	ipopts->tos = &udp->udp_pcb->tos;
	ipopts->ttl = &udp->udp_pcb->ttl;
	ipopts->sndmin = UDP_SNDBUF_MIN;
	ipopts->sndmax = UDP_SNDBUF_MAX;
	ipopts->rcvmin = UDP_RCVBUF_MIN;
	ipopts->rcvmax = UDP_RCVBUF_MAX;
}

/*
 * Set socket options on a UDP socket.
 */
static int
udpsock_setsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t len)
{
	struct udpsock *udp = (struct udpsock *)sock;
	struct ipopts ipopts;
	ip_addr_t ipaddr;
	struct in_addr in_addr;
	struct ifdev *ifdev;
	unsigned int flags;
	uint32_t ifindex;
	uint8_t byte;
	int r, val;

	/*
	 * Unfortunately, we have to duplicate most of the multicast options
	 * rather than sharing them with rawsock at the pktsock level.  The
	 * reason is that each of the PCBs have their own multicast abstraction
	 * functions and so we cannot merge the rest.  Same for getsockopt.
	 */

	switch (level) {
	case IPPROTO_IP:
		if (udpsock_is_ipv6(udp))
			break;

		switch (name) {
		case IP_MULTICAST_IF:
			pktsock_set_mcaware(&udp->udp_pktsock);

			if ((r = sockdriver_copyin_opt(data, &in_addr,
			    sizeof(in_addr), len)) != OK)
				return r;

			ip_addr_set_ip4_u32(&ipaddr, in_addr.s_addr);

			if ((ifdev = ifaddr_map_by_addr(&ipaddr)) == NULL)
				return EADDRNOTAVAIL;

			udp_set_multicast_netif_index(udp->udp_pcb,
			    ifdev_get_index(ifdev));

			return OK;

		case IP_MULTICAST_LOOP:
			pktsock_set_mcaware(&udp->udp_pktsock);

			if ((r = sockdriver_copyin_opt(data, &byte,
			    sizeof(byte), len)) != OK)
				return r;

			flags = udp_flags(udp->udp_pcb);

			if (byte)
				flags |= UDP_FLAGS_MULTICAST_LOOP;
			else
				flags &= ~UDP_FLAGS_MULTICAST_LOOP;

			udp_setflags(udp->udp_pcb, flags);

			return OK;

		case IP_MULTICAST_TTL:
			pktsock_set_mcaware(&udp->udp_pktsock);

			if ((r = sockdriver_copyin_opt(data, &byte,
			    sizeof(byte), len)) != OK)
				return r;

			udp_set_multicast_ttl(udp->udp_pcb, byte);

			return OK;
		}

		break;

	case IPPROTO_IPV6:
		if (!udpsock_is_ipv6(udp))
			break;

		switch (name) {
		case IPV6_MULTICAST_IF:
			pktsock_set_mcaware(&udp->udp_pktsock);

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

			udp_set_multicast_netif_index(udp->udp_pcb, ifindex);

			return OK;

		case IPV6_MULTICAST_LOOP:
			pktsock_set_mcaware(&udp->udp_pktsock);

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < 0 || val > 1)
				return EINVAL;

			flags = udp_flags(udp->udp_pcb);

			if (val)
				flags |= UDP_FLAGS_MULTICAST_LOOP;
			else
				flags &= ~UDP_FLAGS_MULTICAST_LOOP;

			/*
			 * lwIP's IPv6 functionality does not actually check
			 * this flag at all yet.  We set it in the hope that
			 * one day this will magically start working.
			 */
			udp_setflags(udp->udp_pcb, flags);

			return OK;

		case IPV6_MULTICAST_HOPS:
			pktsock_set_mcaware(&udp->udp_pktsock);

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < -1 || val > UINT8_MAX)
				return EINVAL;

			if (val == -1)
				val = 1;

			udp_set_multicast_ttl(udp->udp_pcb, val);

			return OK;
		}

		break;
	}

	/* Handle all other options at the packet or IP level. */
	udpsock_get_ipopts(udp, &ipopts);

	return pktsock_setsockopt(&udp->udp_pktsock, level, name, data, len,
	    &ipopts);
}

/*
 * Retrieve socket options on a UDP socket.
 */
static int
udpsock_getsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t * len)
{
	struct udpsock *udp = (struct udpsock *)sock;
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
		if (udpsock_is_ipv6(udp))
			break;

		switch (name) {
		case IP_MULTICAST_IF:
			ifindex = udp_get_multicast_netif_index(udp->udp_pcb);

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
			flags = udp_flags(udp->udp_pcb);

			byte = !!(flags & UDP_FLAGS_MULTICAST_LOOP);

			return sockdriver_copyout_opt(data, &byte,
			    sizeof(byte), len);

		case IP_MULTICAST_TTL:
			byte = udp_get_multicast_ttl(udp->udp_pcb);

			return sockdriver_copyout_opt(data, &byte,
			    sizeof(byte), len);
		}

		break;

	case IPPROTO_IPV6:
		if (!udpsock_is_ipv6(udp))
			break;

		switch (name) {
		case IPV6_MULTICAST_IF:
			ifindex = udp_get_multicast_netif_index(udp->udp_pcb);

			val = (int)ifindex;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_MULTICAST_LOOP:
			flags = udp_flags(udp->udp_pcb);

			val = !!(flags & UDP_FLAGS_MULTICAST_LOOP);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_MULTICAST_HOPS:
			val = udp_get_multicast_ttl(udp->udp_pcb);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;
	}

	/* Handle all other options at the packet or IP level. */
	udpsock_get_ipopts(udp, &ipopts);

	return pktsock_getsockopt(&udp->udp_pktsock, level, name, data, len,
	    &ipopts);
}

/*
 * Retrieve the local socket address of a UDP socket.
 */
static int
udpsock_getsockname(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct udpsock *udp = (struct udpsock *)sock;

	ipsock_put_addr(udpsock_get_ipsock(udp), addr, addr_len,
	    &udp->udp_pcb->local_ip, udp->udp_pcb->local_port);

	return OK;
}

/*
 * Retrieve the remote socket address of a UDP socket.
 */
static int
udpsock_getpeername(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct udpsock *udp = (struct udpsock *)sock;

	if (!udpsock_is_conn(udp))
		return ENOTCONN;

	ipsock_put_addr(udpsock_get_ipsock(udp), addr, addr_len,
	    &udp->udp_pcb->remote_ip, udp->udp_pcb->remote_port);

	return OK;
}

/*
 * Shut down a UDP socket for reading and/or writing.
 */
static int
udpsock_shutdown(struct sock * sock, unsigned int mask)
{
	struct udpsock *udp = (struct udpsock *)sock;

	if (mask & SFL_SHUT_RD)
		udp_recv(udp->udp_pcb, NULL, NULL);

	pktsock_shutdown(&udp->udp_pktsock, mask);

	return OK;
}

/*
 * Close a UDP socket.
 */
static int
udpsock_close(struct sock * sock, int force __unused)
{
	struct udpsock *udp = (struct udpsock *)sock;

	udp_recv(udp->udp_pcb, NULL, NULL);

	udp_remove(udp->udp_pcb);
	udp->udp_pcb = NULL;

	pktsock_close(&udp->udp_pktsock);

	return OK;
}

/*
 * Free up a closed UDP socket.
 */
static void
udpsock_free(struct sock * sock)
{
	struct udpsock *udp = (struct udpsock *)sock;

	assert(udp->udp_pcb == NULL);

	SIMPLEQ_INSERT_HEAD(&udp_freelist, udp, udp_next);
}

/*
 * Fill the given kinfo_pcb sysctl(7) structure with information about the UDP
 * PCB identified by the given pointer.
 */
static void
udpsock_get_info(struct kinfo_pcb * ki, const void * ptr)
{
	const struct udp_pcb *pcb = (const struct udp_pcb *)ptr;
	struct udpsock *udp;

	ki->ki_type = SOCK_DGRAM;

	/*
	 * All UDP sockets should be created by this module, but protect
	 * ourselves from the case that that is not true anyway.
	 */
	if (pcb->recv_arg != NULL) {
		udp = (struct udpsock *)pcb->recv_arg;

		assert(udp >= udp_array &&
		    udp < &udp_array[__arraycount(udp_array)]);
	} else
		udp = NULL;

	ipsock_get_info(ki, &pcb->local_ip, pcb->local_port, &pcb->remote_ip,
	    pcb->remote_port);

	if (udp != NULL) {
		/* TODO: change this so that sockstat(1) may work one day. */
		ki->ki_sockaddr = (uint64_t)(uintptr_t)udpsock_get_sock(udp);

		ki->ki_rcvq = pktsock_get_recvlen(&udp->udp_pktsock);
	}
}

/*
 * Given either NULL or a previously returned UDP PCB pointer, return the first
 * or next UDP PCB pointer, or NULL if there are no more.  Skip UDP PCBs that
 * are not bound to an address, as there is no use reporting them.
 */
static const void *
udpsock_enum(const void * last)
{
	const struct udp_pcb *pcb;

	if (last != NULL)
		pcb = (const void *)((const struct udp_pcb *)last)->next;
	else
		pcb = (const void *)udp_pcbs;

	while (pcb != NULL && pcb->local_port == 0)
		pcb = pcb->next;

	return pcb;
}

/*
 * Obtain the list of UDP protocol control blocks, for sysctl(7).
 */
static ssize_t
udpsock_pcblist(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{

	return util_pcblist(call, oldp, udpsock_enum, udpsock_get_info);
}

static const struct sockevent_ops udpsock_ops = {
	.sop_bind		= udpsock_bind,
	.sop_connect		= udpsock_connect,
	.sop_pre_send		= udpsock_pre_send,
	.sop_send		= udpsock_send,
	.sop_pre_recv		= pktsock_pre_recv,
	.sop_recv		= pktsock_recv,
	.sop_test_recv		= pktsock_test_recv,
	.sop_ioctl		= ifconf_ioctl,
	.sop_setsockmask	= udpsock_setsockmask,
	.sop_setsockopt		= udpsock_setsockopt,
	.sop_getsockopt		= udpsock_getsockopt,
	.sop_getsockname	= udpsock_getsockname,
	.sop_getpeername	= udpsock_getpeername,
	.sop_shutdown		= udpsock_shutdown,
	.sop_close		= udpsock_close,
	.sop_free		= udpsock_free
};
