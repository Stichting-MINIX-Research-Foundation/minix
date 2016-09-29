/* LWIP service - pktsock.c - packet code shared between UDP and RAW */

#include "lwip.h"
#include "pktsock.h"
#include "ifaddr.h"

/*
 * This buffer should be much bigger (at least 10KB, according to RFC 3542),
 * but we do not support the ancillary options that take so much space anyway.
 */
#define PKTSOCK_CTLBUF_SIZE		256

static char pktsock_ctlbuf[PKTSOCK_CTLBUF_SIZE];

/*
 * Header structures with ancillary data for received packets.  The reason that
 * we do not simply use a generic pkthdr structure with ip_addr_t source and
 * destination addresses, is that for UDP packets, we put this structure in
 * place of the received (ethernet and IP headers), and such a full structure
 * (including IPv6-size addresses) would not fit in the header space for IPv4
 * packets.  So instead we use two address structures, one for IPv4 and one for
 * IPv6, and a generic header structure on top of it, which also identifies
 * which address structure is underneath.  The combination of the address
 * structure and the header structure must fit in the IP header.  The IPv6
 * packet header is already so close to the limit here that we have to use
 * packed addresses.  For IPv4 we use the regular addresses for simplicity.
 */
struct pkthdr {
	uint16_t port;			/* source port number (UDP only) */
	uint8_t dstif;			/* interface that received the pkt */
	uint8_t addrif;			/* interface that accepted the pkt */
	uint8_t tos;			/* TOS/TC value from the IP header */
	uint8_t ttl;			/* TTL/HL value from the IP header */
	uint8_t flags;			/* packet flags (PKTHF_) */
	uint8_t _unused;		/* all that is still available.. */
};

#define PKTHF_IPV6		0x01	/* packet has IPv6 header */
#define PKTHF_MCAST		0x02	/* packet has multicast destination */
#define PKTHF_BCAST		0x04	/* packet has broadcast destination */

struct pktaddr4 {
	ip4_addr_t srcaddr;
	ip4_addr_t dstaddr;
};

struct pktaddr6 {
	ip6_addr_p_t srcaddr;
	ip6_addr_p_t dstaddr;
};

/*
 * Create a packet socket.  Relay parameters and return values to and from the
 * IP module's socket creation function.  This function must not allocate any
 * resources in any form, as socket creation may still fail later, in which
 * case no destruction function is called.
 */
int
pktsock_socket(struct pktsock * pkt, int domain, size_t sndbuf, size_t rcvbuf,
	struct sock ** sockp)
{

	pkt->pkt_rcvhead = NULL;
	pkt->pkt_rcvtailp = &pkt->pkt_rcvhead;
	pkt->pkt_rcvlen = 0;

	mcast_reset(&pkt->pkt_mcast);

	memset(&pkt->pkt_srcaddr, 0, sizeof(pkt->pkt_srcaddr));
	pkt->pkt_ifindex = 0;

	/*
	 * Any PKTF_ type flags should be initialized on the socket only after
	 * the following call, as this call will clear the flags field.  For
	 * now, no PKTF_ flags need to be set by default, though.
	 */
	return ipsock_socket(&pkt->pkt_ipsock, domain, sndbuf, rcvbuf, sockp);
}

/*
 * Return TRUE if the given packet can and should be received on the given
 * socket, or FALSE if there is a reason not to receive the packet.
 */
static int
pktsock_may_recv(struct pktsock * pkt, struct pbuf * pbuf)
{

	/*
	 * By policy, multicast packets should not be received on sockets of
	 * which the owning application is not multicast aware.
	 */
	if (ip_addr_ismulticast(ip_current_dest_addr()) &&
	    !(ipsock_get_flag(&pkt->pkt_ipsock, PKTF_MCAWARE)))
		return FALSE;

	/*
	 * Due to fragment reassembly, we might end up with packets that take
	 * up more buffer space than their byte size, even after rounding up
	 * the latter.  The user probably does not want packets to get dropped
	 * for that reason, e.g. when they set a 64K limit and the packet ends
	 * up being estimated as 65K and dropped.  So, we test against
	 * 'pbuf->tot_len' rather than the rounded-up packet size.  However,
	 * 'pkt->pkt_rcvlen' itself is increased by the rounded-up packet size
	 * when enqueuing the packet, so that we still count the memory
	 * consumption (generally) conservatively, which is what we want.
	 */
	return (pkt->pkt_rcvlen + pbuf->tot_len <=
	    ipsock_get_rcvbuf(&pkt->pkt_ipsock));
}

/*
 * Check whether the given packet can and should be received on the given
 * socket.  If so, return the amount of space for ancillary information that
 * will be necessary for the packet.  If not, return a negative value.
 */
int
pktsock_test_input(struct pktsock * pkt, struct pbuf * pbuf)
{

	/*
	 * This check will be done again in pktsock_input(), but this function
	 * is called for raw packets only (not for UDP packets) and, if this
	 * (cheap) check fails, we can avoid a (rather expensive) packet copy.
	 */
	if (!pktsock_may_recv(pkt, pbuf))
		return -1;

	if (ip_current_is_v6())
		return (int)(sizeof(struct pktaddr6) + sizeof(struct pkthdr));
	else
		return (int)(sizeof(struct pktaddr4) + sizeof(struct pkthdr));
}

/*
 * A packet has arrived on a packet socket.  We own the given packet buffer,
 * and so we must free it if we do not want to keep it.
 */
void
pktsock_input(struct pktsock * pkt, struct pbuf * pbuf,
	const ip_addr_t * srcaddr, uint16_t port)
{
	struct pktaddr4 pktaddr4;
	struct pktaddr6 pktaddr6;
	struct pkthdr pkthdr;
	void *pktaddr;
	struct ifdev *ifdev;
	size_t pktaddrlen;

	/*
	 * We are going to mess with the packet's header and contents, so we
	 * must be the exclusive owner of the packet.  For UDP packets, lwIP
	 * must have made a copy for us in case of non-exclusive delivery
	 * (e.g., multicast packets).  For raw packets, we have made a copy of
	 * the packet ourselves just before the call to this function.
	 */
	if (pbuf->ref != 1)
		panic("input packet has multiple references!");

	/* If the packet should not be received on this socket, drop it. */
	if (!pktsock_may_recv(pkt, pbuf)) {
		pbuf_free(pbuf);

		return;
	}

	/*
	 * Enqueue the packet.  Overwrite the leading IP header with packet
	 * information that is used at the time of receipt by userland.  The
	 * data structures are such that the information always fits in what
	 * was the IP header.  The reference count check earlier ensures that
	 * we never overwrite part of a packet that is still in use elsewhere.
	 */
	if (ip_current_is_v6()) {
		assert(IP_IS_V6(srcaddr));
		assert(ip6_current_dest_addr() != NULL);

		ip6_addr_copy_to_packed(pktaddr6.srcaddr, *ip_2_ip6(srcaddr));
		ip6_addr_copy_to_packed(pktaddr6.dstaddr,
		    *ip6_current_dest_addr());
		pktaddr = &pktaddr6;
		pktaddrlen = sizeof(pktaddr6);

		assert(pktaddrlen + sizeof(pkthdr) <= IP6_HLEN);

		pkthdr.tos = IP6H_TC(ip6_current_header());
		pkthdr.ttl = IP6H_HOPLIM(ip6_current_header());
		pkthdr.flags = PKTHF_IPV6;
	} else {
		assert(IP_IS_V4(srcaddr));
		assert(ip4_current_dest_addr() != NULL);

		memcpy(&pktaddr4.srcaddr, ip_2_ip4(srcaddr),
		    sizeof(pktaddr4.srcaddr));
		memcpy(&pktaddr4.dstaddr, ip4_current_dest_addr(),
		    sizeof(pktaddr4.srcaddr));
		pktaddr = &pktaddr4;
		pktaddrlen = sizeof(pktaddr4);

		assert(pktaddrlen + sizeof(pkthdr) <= IP_HLEN);

		pkthdr.tos = IPH_TOS(ip4_current_header());
		pkthdr.ttl = IPH_TTL(ip4_current_header());
		pkthdr.flags = 0;
	}

	/*
	 * Save both the interface on which the packet was received (for
	 * PKTINFO) and the interface that owns the destination address of the
	 * packet (for the source address's zone ID).
	 */
	assert(ip_current_input_netif() != NULL);
	ifdev = netif_get_ifdev(ip_current_input_netif());
	pkthdr.dstif = (uint16_t)ifdev_get_index(ifdev);

	assert(ip_current_netif() != NULL);
	ifdev = netif_get_ifdev(ip_current_netif());
	pkthdr.addrif = (uint16_t)ifdev_get_index(ifdev);

	if ((pbuf->flags & PBUF_FLAG_LLMCAST) ||
	    ip_addr_ismulticast(ip_current_dest_addr()))
		pkthdr.flags |= PKTHF_MCAST;
	else if ((pbuf->flags & PBUF_FLAG_LLBCAST) ||
	    ip_addr_isbroadcast(ip_current_dest_addr(), ip_current_netif()))
		pkthdr.flags |= PKTHF_BCAST;

	pkthdr.port = port;

	util_pbuf_header(pbuf, sizeof(pkthdr));

	memcpy(pbuf->payload, &pkthdr, sizeof(pkthdr));

	util_pbuf_header(pbuf, pktaddrlen);

	memcpy(pbuf->payload, pktaddr, pktaddrlen);

	util_pbuf_header(pbuf, -(int)(sizeof(pkthdr) + pktaddrlen));

	*pkt->pkt_rcvtailp = pbuf;
	pkt->pkt_rcvtailp = pchain_end(pbuf);
	pkt->pkt_rcvlen += pchain_size(pbuf);

	sockevent_raise(ipsock_get_sock(&pkt->pkt_ipsock), SEV_RECV);
}

/*
 * Obtain interface and source address information for an outgoing packet.  In
 * particular, parse any IPV6_PKTINFO options provided as either sticky options
 * on the socket 'pkt' or as ancillary options in the packet options 'pkto'.
 * On success, return OK, with 'ifdevp' set to either the outgoing interface to
 * use for the packet, or NULL if no outgoing interface was specified using
 * either of the aforementioned options.  If, and only if, 'ifdevp' is set to
 * an actual interface (i.e., not NULL), then 'src_addrp' is filled with either
 * a locally owned, validated, unicast address to use as source of the packet,
 * or the unspecified ('any') address if no source address was specified using
 * the options.  On failure, return a negative error code.
 */
int
pktsock_get_pktinfo(struct pktsock * pkt, struct pktopt * pkto,
	struct ifdev ** ifdevp, ip_addr_t * src_addrp)
{
	struct ifdev *ifdev, *ifdev2;
	ip_addr_t ipaddr;
	uint32_t ifindex;
	int r;

	/* We support only IPV6_PKTINFO.  IP_PKTINFO is not supported. */
	if (!ipsock_is_ipv6(&pkt->pkt_ipsock)) {
		*ifdevp = NULL;
		return OK;
	}

	/*
	 * TODO: we are spending a lot of effort on initializing and copying
	 * stuff around, even just to find out whether there is anything to do
	 * at all here.  See if this can be optimized.
	 */
	ip_addr_set_zero_ip6(&ipaddr);

	/*
	 * Ancillary data takes precedence over sticky options.  We treat the
	 * source address and interface index fields as separate, overriding
	 * each earlier value only if non-zero.  TODO: is that correct?
	 */
	if (pkto->pkto_flags & PKTOF_PKTINFO) {
		memcpy(ip_2_ip6(&ipaddr)->addr, &pkto->pkto_srcaddr.addr,
		    sizeof(ip_2_ip6(&ipaddr)->addr));
		ifindex = pkto->pkto_ifindex;
	} else
		ifindex = 0;

	if (ip6_addr_isany(ip_2_ip6(&ipaddr)))
		memcpy(ip_2_ip6(&ipaddr)->addr, &pkt->pkt_srcaddr.addr,
		    sizeof(ip_2_ip6(&ipaddr)->addr));
	if (ifindex == 0)
		ifindex = pkt->pkt_ifindex;

	/* If both fields are blank, there is nothing more to do. */
	if (ip6_addr_isany(ip_2_ip6(&ipaddr)) && ifindex == 0) {
		*ifdevp = NULL;
		return OK;
	}

	/* If an interface index is specified, it must be valid. */
	ifdev = NULL;

	if (ifindex != 0 && (ifdev = ifdev_get_by_index(ifindex)) == NULL)
		return ENXIO;

	/*
	 * Use the interface index to set a zone on the source address, if the
	 * source address has a scope.
	 */
	if (ip6_addr_has_scope(ip_2_ip6(&ipaddr), IP6_UNKNOWN)) {
		if (ifindex == 0)
			return EADDRNOTAVAIL;

		ip6_addr_set_zone(ip_2_ip6(&ipaddr), ifindex);
	}

	/*
	 * We need to validate the given address just as thoroughly as an
	 * address given through bind().  If we don't, we could allow forged
	 * source addresses etcetera.  To be sure: this call may change the
	 * address to an IPv4 type address if needed.
	 */
	if ((r = ipsock_check_src_addr(pktsock_get_ipsock(pkt), &ipaddr,
	    FALSE /*allow_mcast*/, &ifdev2)) != OK)
		return r;

	if (ifdev2 != NULL) {
		if (ifdev == NULL)
			ifdev = ifdev2;
		else if (ifdev != ifdev2)
			return EADDRNOTAVAIL;
	} else {
		/*
		 * There should be no cases where the (non-multicast) address
		 * successfully parsed, is not unspecified, and yet did not map
		 * to an interface.  Eliminate the possibility anyway by
		 * throwing an error for this case.  As a result, we are left
		 * with one of two cases:
		 *
		 * 1) ifdevp is not NULL, and src_addrp is unspecified;
		 * 2) ifdevp is not NULL, and src_addrp is a locally assigned
		 *    (unicast) address.
		 *
		 * This is why we need not fill src_addrp when ifdevp is NULL.
		 */
		if (!ip_addr_isany(&ipaddr))
			return EADDRNOTAVAIL;
	}

	*ifdevp = ifdev;
	if (ifdev != NULL)
		*src_addrp = ipaddr;
	return OK;
}

/*
 * Parse a chunk of user-provided control data, on an IPv4 socket provided as
 * 'pkt'.  The control chunk is given as 'cmsg', and the length of the data
 * following the control header (possibly zero) is given as 'len'.  On success,
 * return OK, with any parsed options merged into the set of packet options
 * 'pkto'.  On failure, return a negative error code.
 */
static int
pktsock_parse_ctl_v4(struct pktsock * pkt __unused, struct cmsghdr * cmsg,
	socklen_t len, struct pktopt * pkto)
{
	uint8_t byte;
	int val;

	if (cmsg->cmsg_level != IPPROTO_IP)
		return EAFNOSUPPORT;

	switch (cmsg->cmsg_type) {
	case IP_TOS:
		/*
		 * Some userland code (bind's libisc in particular) supplies
		 * a single byte instead of a full integer for this option.
		 * We go out of our way to accept that format, too.
		 */
		if (len != sizeof(val) && len != sizeof(byte))
			return EINVAL;

		if (len == sizeof(byte)) {
			memcpy(&byte, CMSG_DATA(cmsg), sizeof(byte));
			val = (int)byte;
		} else
			memcpy(&val, CMSG_DATA(cmsg), sizeof(val));

		if (val < 0 || val > UINT8_MAX)
			return EINVAL;

		pkto->pkto_flags |= PKTOF_TOS;
		pkto->pkto_tos = (uint8_t)val;

		return OK;

	case IP_TTL:
		if (len != sizeof(val))
			return EINVAL;

		memcpy(&val, CMSG_DATA(cmsg), sizeof(val));

		if (val < 0 || val > UINT8_MAX)
			return EINVAL;

		pkto->pkto_flags |= PKTOF_TTL;
		pkto->pkto_ttl = (uint8_t)val;

		return OK;

	/*
	 * Implementing IP_PKTINFO might be a bit harder than its IPV6_PKTINFO
	 * sibling, because it would require the use of zone IDs (interface
	 * indices) for IPv4, which is not supported yet.
	 */
	}

	return EINVAL;
}

/*
 * Parse a chunk of user-provided control data, on an IPv6 socket provided as
 * 'pkt'.  The control chunk is given as 'cmsg', and the length of the data
 * following the control header (possibly zero) is given as 'len'.  On success,
 * return OK, with any parsed options merged into the set of packet options
 * 'pkto'.  On failure, return a negative error code.
 */
static int
pktsock_parse_ctl_v6(struct pktsock * pkt, struct cmsghdr * cmsg,
	socklen_t len, struct pktopt * pkto)
{
	struct in6_pktinfo ipi6;
	int val;

	if (cmsg->cmsg_level != IPPROTO_IPV6)
		return EAFNOSUPPORT;

	switch (cmsg->cmsg_type) {
	case IPV6_TCLASS:
		if (len != sizeof(val))
			return EINVAL;

		memcpy(&val, CMSG_DATA(cmsg), sizeof(val));

		if (val < -1 || val > UINT8_MAX)
			return EINVAL;

		if (val == -1)
			val = 0;

		pkto->pkto_flags |= PKTOF_TOS;
		pkto->pkto_tos = (uint8_t)val;

		return OK;

	case IPV6_HOPLIMIT:
		if (len != sizeof(val))
			return EINVAL;

		memcpy(&val, CMSG_DATA(cmsg), sizeof(val));

		if (val < -1 || val > UINT8_MAX)
			return EINVAL;

		if (val == -1)
			val = IP_DEFAULT_TTL;

		pkto->pkto_flags |= PKTOF_TTL;
		pkto->pkto_ttl = (uint8_t)val;

		return OK;

	case IPV6_PKTINFO:
		if (len != sizeof(ipi6))
			return EINVAL;

		memcpy(&ipi6, CMSG_DATA(cmsg), sizeof(ipi6));

		pkto->pkto_flags |= PKTOF_PKTINFO;
		memcpy(&pkto->pkto_srcaddr.addr, &ipi6.ipi6_addr,
		    sizeof(pkto->pkto_srcaddr.addr));
		pkto->pkto_ifindex = ipi6.ipi6_ifindex;

		return OK;

	case IPV6_USE_MIN_MTU:
		if (len != sizeof(int))
			return EINVAL;

		memcpy(&val, CMSG_DATA(cmsg), sizeof(val));

		if (val < -1 || val > 1)
			return EINVAL;

		/* TODO: not supported by lwIP, but needed by applications. */
		return OK;
	}

	return EINVAL;
}

/*
 * Copy in and parse control data, as part of sending a packet on socket 'pkt'.
 * The control data is accessible through 'ctl', with a user-provided length of
 * 'ctl_len'.  On success, return OK, with any parsed packet options stored in
 * 'pkto'.  On failure, return a negative error code.
 */
int
pktsock_get_ctl(struct pktsock * pkt, const struct sockdriver_data * ctl,
	socklen_t ctl_len, struct pktopt * pkto)
{
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	socklen_t left, len;
	int r;

	/* The default: no packet options are being overridden. */
	assert(pkto->pkto_flags == 0);

	/* If no control length is given, we are done here. */
	if (ctl_len == 0)
		return OK;

	/*
	 * For now, we put a rather aggressive limit on the size of the control
	 * data.  We copy in and parse the whole thing in a single buffer.
	 */
	if (ctl_len > sizeof(pktsock_ctlbuf)) {
		printf("LWIP: too much control data given (%u bytes)\n",
		    ctl_len);

		return ENOBUFS;
	}

	if ((r = sockdriver_copyin(ctl, 0, pktsock_ctlbuf, ctl_len)) != OK)
		return r;

	memset(&msghdr, 0, sizeof(msghdr));
	msghdr.msg_control = pktsock_ctlbuf;
	msghdr.msg_controllen = ctl_len;

	for (cmsg = CMSG_FIRSTHDR(&msghdr); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
		/* Check for bogus lengths. */
		assert((socklen_t)((char *)cmsg - pktsock_ctlbuf) <= ctl_len);
		left = ctl_len - (socklen_t)((char *)cmsg - pktsock_ctlbuf);
		assert(left >= CMSG_LEN(0)); /* guaranteed by CMSG_xxHDR */

		if (cmsg->cmsg_len < CMSG_LEN(0) || cmsg->cmsg_len > left) {
			printf("LWIP: malformed control data rejected\n");

			return EINVAL;
		}

		len = cmsg->cmsg_len - CMSG_LEN(0);

		if (ipsock_is_ipv6(&pkt->pkt_ipsock))
			r = pktsock_parse_ctl_v6(pkt, cmsg, len, pkto);
		else
			r = pktsock_parse_ctl_v4(pkt, cmsg, len, pkto);

		if (r != OK)
			return r;
	}

	return OK;
}

/*
 * Copy in the packet data from the calling user process, and store it in the
 * buffer 'pbuf' that must already have been allocated with the appropriate
 * size.
 */
int
pktsock_get_data(struct pktsock * pkt, const struct sockdriver_data * data,
	size_t len, struct pbuf * pbuf)

{

	return util_copy_data(data, len, 0, pbuf, 0, TRUE /*copy_in*/);
}

/*
 * Dequeue and free the head of the receive queue of a packet socket.
 */
static void
pktsock_dequeue(struct pktsock * pkt)
{
	struct pbuf *pbuf, **pnext;
	size_t size;

	pbuf = pkt->pkt_rcvhead;
	assert(pbuf != NULL);

	pnext = pchain_end(pbuf);
	size = pchain_size(pbuf);

	if ((pkt->pkt_rcvhead = *pnext) == NULL)
		pkt->pkt_rcvtailp = &pkt->pkt_rcvhead;

	assert(pkt->pkt_rcvlen >= size);
	pkt->pkt_rcvlen -= size;

	*pnext = NULL;
	pbuf_free(pbuf);
}

/*
 * Perform preliminary checks on a receive request.
 */
int
pktsock_pre_recv(struct sock * sock __unused, endpoint_t user_endpt __unused,
	int flags)
{

	/*
	 * We accept the same flags across all socket types in LWIP, and then
	 * simply ignore the ones we do not support for packet sockets.
	 */
	if ((flags & ~(MSG_PEEK | MSG_WAITALL)) != 0)
		return EOPNOTSUPP;

	return OK;
}

/*
 * Add a chunk of control data to the global control buffer, starting from
 * offset 'off'.  The chunk has the given level and type, and its data is given
 * in the buffer 'ptr' with size 'len'.  Return the (padded) size of the chunk
 * that was generated as a result.
 */
static size_t
pktsock_add_ctl(int level, int type, void * ptr, socklen_t len, size_t off)
{
	struct cmsghdr cmsg;
	size_t size;

	size = CMSG_SPACE(len);

	/*
	 * The global control buffer must be large enough to store one chunk
	 * of each of the supported options.  If this panic triggers, increase
	 * PKTSOCK_CTLBUF_SIZE by as much as needed.
	 */
	if (off + size > sizeof(pktsock_ctlbuf))
		panic("control buffer too small, increase "
		    "PKTSOCK_CTLBUF_SIZE");

	memset(&cmsg, 0, sizeof(cmsg));
	cmsg.cmsg_len = CMSG_LEN(len);
	cmsg.cmsg_level = level;
	cmsg.cmsg_type = type;

	/*
	 * Clear any padding space.  This can be optimized, but in any case we
	 * must be careful not to copy out any bytes that have not been
	 * initialized at all.
	 */
	memset(&pktsock_ctlbuf[off], 0, size);

	memcpy(&pktsock_ctlbuf[off], &cmsg, sizeof(cmsg));
	memcpy(CMSG_DATA((struct cmsghdr *)&pktsock_ctlbuf[off]), ptr, len);

	return size;
}

/*
 * Generate and copy out control data, as part of delivering a packet from
 * socket 'pkt' to userland.  The control data buffer is given as 'ctl', with
 * a user-given length of 'ctl_len' bytes.  The packet's header information is
 * provided as 'pkthdr', and its source and destination addresses as 'pktaddr',
 * which maybe a pktaddr4 or pktaddr6 structure depending on the value of the
 * PKTHF_IPV6 flag in the 'flags' field in 'pkthdr'.  Note that we support
 * dual-stack sockets, and as such it is possible that the socket is of domain
 * AF_INET6 while the received packet is an IPv4 packet.  On success, return
 * the size of the control data copied out (possibly zero).  If more control
 * data were generated than copied out, also merge the MSG_CTRUNC flag into
 * 'rflags'.  On failure, return a negative error code.
 */
static int
pktsock_put_ctl(struct pktsock * pkt, const struct sockdriver_data * ctl,
	socklen_t ctl_len, struct pkthdr * pkthdr, void * pktaddr,
	int * rflags)
{
	struct pktaddr6 *pktaddr6;
	struct pktaddr4 *pktaddr4;
	struct in_pktinfo ipi;
	struct in6_pktinfo ipi6;
	ip_addr_t ipaddr;
	unsigned int flags;
	uint8_t byte;
	size_t off;
	int r, val;

	flags = ipsock_get_flags(&pkt->pkt_ipsock);

	if (!(flags & (PKTF_RECVINFO | PKTF_RECVTOS | PKTF_RECVTTL)))
		return 0;

	/*
	 * Important: all generated control chunks must fit in the global
	 * control buffer together.  When adding more options here, ensure that
	 * the control buffer remains large enough to receive all options at
	 * once.  See also the panic in pktsock_add_ctl().
	 */
	off = 0;

	/*
	 * IPv6 sockets may receive IPv4 packets.  The ancillary data is in the
	 * format corresponding to the socket, which means we may have to
	 * convert any IPv4 addresses from the packet to IPv4-mapped IPv6
	 * addresses for the ancillary data, just like the source address.
	 */
	if (ipsock_is_ipv6(&pkt->pkt_ipsock)) {
		if (flags & PKTF_RECVTTL) {
			val = pkthdr->ttl;

			off += pktsock_add_ctl(IPPROTO_IPV6, IPV6_HOPLIMIT,
			    &val, sizeof(val), off);
		}

		if (flags & PKTF_RECVTOS) {
			val = pkthdr->tos;

			off += pktsock_add_ctl(IPPROTO_IPV6, IPV6_TCLASS, &val,
			    sizeof(val), off);
		}

		if (flags & PKTF_RECVINFO) {
			memset(&ipi6, 0, sizeof(ipi6));

			if (pkthdr->flags & PKTHF_IPV6) {
				pktaddr6 = (struct pktaddr6 *)pktaddr;
				memcpy(&ipi6.ipi6_addr, &pktaddr6->dstaddr,
				    sizeof(ipi6.ipi6_addr));
			} else {
				pktaddr4 = (struct pktaddr4 *)pktaddr;

				addr_make_v4mapped_v6(&ipaddr,
				    &pktaddr4->dstaddr);

				memcpy(&ipi6.ipi6_addr,
				    ip_2_ip6(&ipaddr)->addr,
				    sizeof(ipi6.ipi6_addr));
			}
			ipi6.ipi6_ifindex = pkthdr->dstif;

			off += pktsock_add_ctl(IPPROTO_IPV6, IPV6_PKTINFO,
			    &ipi6, sizeof(ipi6), off);
		}
	} else {
		if (flags & PKTF_RECVTTL) {
			byte = pkthdr->ttl;

			off += pktsock_add_ctl(IPPROTO_IP, IP_TTL, &byte,
			    sizeof(byte), off);
		}

		if (flags & PKTF_RECVINFO) {
			assert(!(pkthdr->flags & PKTHF_IPV6));
			pktaddr4 = (struct pktaddr4 *)pktaddr;

			memset(&ipi, 0, sizeof(ipi));
			memcpy(&ipi.ipi_addr, &pktaddr4->dstaddr,
			    sizeof(ipi.ipi_addr));
			ipi.ipi_ifindex = pkthdr->dstif;

			off += pktsock_add_ctl(IPPROTO_IP, IP_PKTINFO, &ipi,
			    sizeof(ipi), off);
		}
	}

	assert(off > 0);

	if (ctl_len >= off)
		ctl_len = off;
	else
		*rflags |= MSG_CTRUNC;

	if (ctl_len > 0 &&
	    (r = sockdriver_copyout(ctl, 0, pktsock_ctlbuf, ctl_len)) != OK)
		return r;

	return ctl_len;
}

/*
 * Receive data on a packet socket.
 */
int
pktsock_recv(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl,
	socklen_t ctl_len, socklen_t * ctl_off, struct sockaddr * addr,
	socklen_t * addr_len, endpoint_t user_endpt __unused, int flags,
	size_t min __unused, int * rflags)
{
	struct pktsock *pkt = (struct pktsock *)sock;
	struct pktaddr4 pktaddr4;
	struct pktaddr6 pktaddr6;
	struct pkthdr pkthdr;
	void *pktaddr;
	struct pbuf *pbuf;
	ip_addr_t srcaddr;
	int r;

	if ((pbuf = pkt->pkt_rcvhead) == NULL)
		return SUSPEND;

	/*
	 * Get the ancillary data for the packet.  The format of the ancillary
	 * data depends on the received packet type, which may be different
	 * from the socket type.
	 */
	util_pbuf_header(pbuf, sizeof(pkthdr));

	memcpy(&pkthdr, pbuf->payload, sizeof(pkthdr));

	if (pkthdr.flags & PKTHF_IPV6) {
		util_pbuf_header(pbuf, sizeof(pktaddr6));

		memcpy(&pktaddr6, pbuf->payload, sizeof(pktaddr6));
		pktaddr = &pktaddr6;

		ip_addr_copy_from_ip6_packed(srcaddr, pktaddr6.srcaddr);
		if (ip6_addr_has_scope(ip_2_ip6(&srcaddr), IP6_UNICAST))
			ip6_addr_set_zone(ip_2_ip6(&srcaddr), pkthdr.addrif);

		util_pbuf_header(pbuf,
		    -(int)(sizeof(pkthdr) + sizeof(pktaddr6)));
	} else {
		util_pbuf_header(pbuf, sizeof(pktaddr4));

		memcpy(&pktaddr4, pbuf->payload, sizeof(pktaddr4));
		pktaddr = &pktaddr4;

		ip_addr_copy_from_ip4(srcaddr, pktaddr4.srcaddr);

		util_pbuf_header(pbuf,
		    -(int)(sizeof(pkthdr) + sizeof(pktaddr4)));
	}

	/* Copy out the packet data to the calling user process. */
	if (len >= pbuf->tot_len)
		len = pbuf->tot_len;
	else
		*rflags |= MSG_TRUNC;

	r = util_copy_data(data, len, 0, pbuf, 0, FALSE /*copy_in*/);

	if (r != OK)
		return r;

	/* Generate and copy out ancillary (control) data, if requested. */
	if ((r = pktsock_put_ctl(pkt, ctl, ctl_len, &pkthdr, pktaddr,
	    rflags)) < 0)
		return r;

	/* Store the source IP address. */
	ipsock_put_addr(&pkt->pkt_ipsock, addr, addr_len, &srcaddr,
	    pkthdr.port);

	/* Set multicast or broadcast message flag, if applicable. */
	if (pkthdr.flags & PKTHF_MCAST)
		*rflags |= MSG_MCAST;
	else if (pkthdr.flags & PKTHF_BCAST)
		*rflags |= MSG_BCAST;

	/* Discard the packet now, unless we were instructed to peek only. */
	if (!(flags & MSG_PEEK))
		pktsock_dequeue(pkt);

	/* Return the received part of the packet length. */
	*off = len;
	*ctl_off = r;
	return OK;
}

/*
 * Test whether data can be received on a packet socket, and if so, how many
 * bytes of data.
 */
int
pktsock_test_recv(struct sock * sock, size_t min __unused, size_t * size)
{
	struct pktsock *pkt = (struct pktsock *)sock;

	if (pkt->pkt_rcvhead == NULL)
		return SUSPEND;

	if (size != NULL)
		*size = pkt->pkt_rcvhead->tot_len;
	return OK;
}

/*
 * The caller has performed a multicast operation on the given socket.  Thus,
 * the caller is multicast aware.  Remember this, because that means the socket
 * may also receive traffic to multicast destinations.
 */
void
pktsock_set_mcaware(struct pktsock * pkt)
{

	ipsock_set_flag(&pkt->pkt_ipsock, PKTF_MCAWARE);
}

/*
 * Set socket options on a packet socket.
 */
int
pktsock_setsockopt(struct pktsock * pkt, int level, int name,
	const struct sockdriver_data * data, socklen_t len,
	struct ipopts * ipopts)
{
	struct ip_mreq imr;
	struct ipv6_mreq ipv6mr;
	struct in6_pktinfo ipi6;
	ip_addr_t ipaddr, ifaddr;
	struct ifdev *ifdev;
	unsigned int flag;
	uint32_t ifindex;
	int r, val, has_scope;

	switch (level) {
	case IPPROTO_IP:
		if (ipsock_is_ipv6(&pkt->pkt_ipsock))
			break;

		switch (name) {
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			pktsock_set_mcaware(pkt);

			if ((r = sockdriver_copyin_opt(data, &imr, sizeof(imr),
			    len)) != OK)
				return r;

			ip_addr_set_ip4_u32(&ipaddr, imr.imr_multiaddr.s_addr);
			ip_addr_set_ip4_u32(&ifaddr, imr.imr_interface.s_addr);

			if (!ip_addr_isany(&ifaddr)) {
				ifdev = ifaddr_map_by_addr(&ifaddr);

				if (ifdev == NULL)
					return EADDRNOTAVAIL;
			} else
				ifdev = NULL;

			if (name == IP_ADD_MEMBERSHIP)
				r = mcast_join(&pkt->pkt_mcast, &ipaddr,
				    ifdev);
			else
				r = mcast_leave(&pkt->pkt_mcast, &ipaddr,
				    ifdev);

			return r;

		case IP_RECVTTL:
		case IP_RECVPKTINFO:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			switch (name) {
			case IP_RECVTTL:	flag = PKTF_RECVTTL; break;
			case IP_RECVPKTINFO:	flag = PKTF_RECVINFO; break;
			default:		flag = 0; assert(0); break;
			}

			if (val)
				ipsock_set_flag(&pkt->pkt_ipsock, flag);
			else
				ipsock_clear_flag(&pkt->pkt_ipsock, flag);

			return OK;
		}

		break;

	case IPPROTO_IPV6:
		if (!ipsock_is_ipv6(&pkt->pkt_ipsock))
			break;

		switch (name) {
		case IPV6_JOIN_GROUP:
		case IPV6_LEAVE_GROUP:
			pktsock_set_mcaware(pkt);

			if ((r = sockdriver_copyin_opt(data, &ipv6mr,
			    sizeof(ipv6mr), len)) != OK)
				return r;

			ip_addr_set_zero_ip6(&ipaddr);
			memcpy(ip_2_ip6(&ipaddr)->addr,
			    &ipv6mr.ipv6mr_multiaddr,
			    sizeof(ip_2_ip6(&ipaddr)->addr));

			/*
			 * We currently do not support joining IPv4 multicast
			 * groups on IPv6 sockets.  The reason for this is that
			 * this would require decisions on what to do if the
			 * socket is set to V6ONLY later, as well as various
			 * additional exceptions for a case that hopefully
			 * doesn't occur in practice anyway.
			 */
			if (ip6_addr_isipv4mappedipv6(ip_2_ip6(&ipaddr)))
				return EADDRNOTAVAIL;

			has_scope = ip6_addr_has_scope(ip_2_ip6(&ipaddr),
			    IP6_UNKNOWN);

			if ((ifindex = ipv6mr.ipv6mr_interface) != 0) {
				ifdev = ifdev_get_by_index(ifindex);

				if (ifdev == NULL)
					return ENXIO;

				if (has_scope)
					ip6_addr_set_zone(ip_2_ip6(&ipaddr),
					    ifindex);
			} else {
				if (has_scope)
					return EADDRNOTAVAIL;

				ifdev = NULL;
			}

			if (name == IPV6_JOIN_GROUP)
				r = mcast_join(&pkt->pkt_mcast, &ipaddr,
				    ifdev);
			else
				r = mcast_leave(&pkt->pkt_mcast, &ipaddr,
				    ifdev);

			return r;

		case IPV6_USE_MIN_MTU:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < -1 || val > 1)
				return EINVAL;

			/*
			 * lwIP does not support path MTU discovery, so do
			 * nothing.  TODO: see if this is actually good enough.
			 */
			return OK;

		case IPV6_PKTINFO:
			if ((r = sockdriver_copyin_opt(data, &ipi6,
			    sizeof(ipi6), len)) != OK)
				return r;

			/*
			 * Simply copy in what is given.  The values will be
			 * parsed only once a packet is sent, in
			 * pktsock_get_pktinfo().  Otherwise, if we perform
			 * checks here, they may be outdated by the time the
			 * values are actually used.
			 */
			memcpy(&pkt->pkt_srcaddr.addr, &ipi6.ipi6_addr,
			    sizeof(pkt->pkt_srcaddr.addr));
			pkt->pkt_ifindex = ipi6.ipi6_ifindex;

			return OK;

		case IPV6_RECVPKTINFO:
		case IPV6_RECVHOPLIMIT:
		case IPV6_RECVTCLASS:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			switch (name) {
			case IPV6_RECVPKTINFO:	flag = PKTF_RECVINFO; break;
			case IPV6_RECVHOPLIMIT:	flag = PKTF_RECVTTL; break;
			case IPV6_RECVTCLASS:	flag = PKTF_RECVTOS; break;
			default:		flag = 0; assert(0); break;
			}

			if (val)
				ipsock_set_flag(&pkt->pkt_ipsock, flag);
			else
				ipsock_clear_flag(&pkt->pkt_ipsock, flag);

			return OK;
		}

		break;
	}

	return ipsock_setsockopt(&pkt->pkt_ipsock, level, name, data, len,
	    ipopts);
}

/*
 * Retrieve socket options on a packet socket.
 */
int
pktsock_getsockopt(struct pktsock * pkt, int level, int name,
	const struct sockdriver_data * data, socklen_t * len,
	struct ipopts * ipopts)
{
	struct in6_pktinfo ipi6;
	unsigned int flag;
	int val;

	switch (level) {
	case IPPROTO_IP:
		if (ipsock_is_ipv6(&pkt->pkt_ipsock))
			break;

		switch (name) {
		case IP_RECVTTL:
		case IP_RECVPKTINFO:
			switch (name) {
			case IP_RECVTTL:	flag = PKTF_RECVTTL; break;
			case IP_RECVPKTINFO:	flag = PKTF_RECVINFO; break;
			default:		flag = 0; assert(0); break;
			}

			val = !!(ipsock_get_flag(&pkt->pkt_ipsock, flag));

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;

	case IPPROTO_IPV6:
		if (!ipsock_is_ipv6(&pkt->pkt_ipsock))
			break;

		switch (name) {
		case IPV6_USE_MIN_MTU:
			/*
			 * TODO: sort out exactly what lwIP actually supports
			 * in the way of path MTU discovery.  Value 1 means
			 * that path MTU discovery is disabled and packets are
			 * sent at the minimum MTU (RFC 3542).
			 */
			val = 1;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case IPV6_PKTINFO:
			memset(&ipi6, 0, sizeof(ipi6));

			/*
			 * Simply copy out whatever was given before.  These
			 * fields are initialized to zero on socket creation.
			 */
			memcpy(&ipi6.ipi6_addr, &pkt->pkt_srcaddr.addr,
			    sizeof(ipi6.ipi6_addr));
			ipi6.ipi6_ifindex = pkt->pkt_ifindex;

			return sockdriver_copyout_opt(data, &ipi6,
			    sizeof(ipi6), len);

		case IPV6_RECVPKTINFO:
		case IPV6_RECVHOPLIMIT:
		case IPV6_RECVTCLASS:
			switch (name) {
			case IPV6_RECVPKTINFO:	flag = PKTF_RECVINFO; break;
			case IPV6_RECVHOPLIMIT:	flag = PKTF_RECVTTL; break;
			case IPV6_RECVTCLASS:	flag = PKTF_RECVTOS; break;
			default:		flag = 0; assert(0); break;
			}

			val = !!(ipsock_get_flag(&pkt->pkt_ipsock, flag));

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;
	}

	return ipsock_getsockopt(&pkt->pkt_ipsock, level, name, data, len,
	    ipopts);
}

/*
 * Drain the receive queue of a packet socket.
 */
static void
pktsock_drain(struct pktsock * pkt)
{

	while (pkt->pkt_rcvhead != NULL)
		pktsock_dequeue(pkt);

	assert(pkt->pkt_rcvlen == 0);
	assert(pkt->pkt_rcvtailp == &pkt->pkt_rcvhead);
}

/*
 * Shut down a packet socket for reading and/or writing.
 */
void
pktsock_shutdown(struct pktsock * pkt, unsigned int mask)
{

	if (mask & SFL_SHUT_RD)
		pktsock_drain(pkt);
}

/*
 * Close a packet socket.
 */
void
pktsock_close(struct pktsock * pkt)
{

	pktsock_drain(pkt);

	mcast_leave_all(&pkt->pkt_mcast);
}

/*
 * Return the rounded-up number of bytes in the packet socket's receive queue,
 * for sysctl(7).  NetBSD returns the used portion of each buffer, but that
 * would be quite some extra effort for us (TODO).
 */
size_t
pktsock_get_recvlen(struct pktsock * pkt)
{

	return pkt->pkt_rcvlen;
}
