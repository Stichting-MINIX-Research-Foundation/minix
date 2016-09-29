/* LWIP service - ethif.c - ethernet interfaces */
/*
 * The most important aspect of this module is to maintain a send queue for the
 * interface.  This send queue consists of packets to send.  At times, the user
 * may request a change to the driver configuration.  While configuration
 * requests would ideally be enqueued in the send queue, this has proven too
 * problematic to work in practice, especially since out-of-memory conditions
 * may prevent configuration requests from being accepted immediately in such a
 * model.  Instead, we take a simple and blunt approach: configuration requests
 * "cut in line" and thus take precedence over pending packets in the send
 * queue.  This may not always be entirely correct: for example, packets may be
 * transmitted with the old ethernet address after the network device has
 * already been reconfigured to receive from a new ethernet address.  However,
 * this should not be a real problem, and we take care explicitly of perhaps
 * the most problematic case: packets not getting checksummed due to checksum
 * offloading configuration changes.
 *
 * Even with this blunt approach, we maintain three concurrent configurations:
 * the active, the pending, and the wanted configuration.  The active one is
 * the last known active configuration at the network driver.  It used not only
 * to report whether the device is in RUNNING state, but also to replay the
 * active configuration to a restarted driver.  The pending configuration is
 * a partially new configuration that has been given to ndev to send to the
 * driver, but not yet acknowledged by the driver.  Finally, the wanted
 * configuration is the latest one that has yet to be given to ndev.
 *
 * Each configuration has a bitmask indicating which part of the configuration
 * has changed, in order to limit work on the driver side.  This is also the
 * reason that the pending and wanted configurations are separate: if e.g. a
 * media change is pending at the driver, and the user also requests a mode
 * change, we do not want the media change to be repeated after it has been
 * acknowleged by the driver, just to change the mode as well.  In this example
 * the pending configuration will have NDEV_SET_MEDIA set, and the wanted
 * configuration will have NDEV_SET_MODE set.  Once acknowledged, the pending
 * bitmask is cleared and the wanted bitmask is tested to see if another
 * configuration change should be given to ndev.  Technically, this could lead
 * to starvation of actual packet transmission, but we expect configuration
 * changes to be very rare, since they are always user initiated.
 *
 * It is important to note for understanding the code that for some fields
 * (mode, flags, caps), the three configurations are cascading: even though the
 * wanted configuration may not have NDEV_SET_MODE set, its mode field will
 * still contain the most recently requested mode; that is, the mode in the
 * pending configuration if that one has NDEV_SET_MODE set, or otherwise the
 * mode in the active configuration.  For that reason, we carefully merge
 * configuration requests into the next level (wanted -> pending -> active),
 * updating just the fields that have been changed by the previous level.  This
 * approach simplifies obtaining current values a lot, but is not very obvious.
 *
 * Also, we never send multiple configuration requests at once, even though
 * ndev would let us do that: we use a single array for the list of multicast
 * ethernet addresses that we send to the driver, which the driver may retrieve
 * (using a memory grant) at any time.  We necessarily recompute the multicast
 * list before sending a configuration request, and thus, sending multiple
 * requests at once may lead to the driver retrieving a corrupted list.
 */

#include "lwip.h"
#include "ethif.h"

#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/igmp.h"
#include "lwip/mld6.h"

#include <net/if_media.h>

#define ETHIF_MAX_MTU	1500		/* maximum MTU value for ethernet */
#define ETHIF_DEF_MTU	ETHIF_MAX_MTU	/* default MTU value that we use */

#define ETHIF_MCAST_MAX	8	/* maximum number of multicast addresses */

struct ethif {
	struct ifdev ethif_ifdev;	/* interface device, MUST be first */
	ndev_id_t ethif_ndev;		/* network device ID */
	unsigned int ethif_flags;	/* interface flags (ETHIFF_) */
	uint32_t ethif_caps;		/* driver capabilities (NDEV_CAPS_) */
	uint32_t ethif_media;		/* driver-reported media type (IFM_) */
	struct ndev_conf ethif_active;	/* active configuration (at driver) */
	struct ndev_conf ethif_pending; /* pending configuration (at ndev) */
	struct ndev_conf ethif_wanted;	/* desired configuration (waiting) */
	struct ndev_hwaddr ethif_mclist[ETHIF_MCAST_MAX]; /* multicast list */
	struct {			/* send queue (packet/conf refs) */
		struct pbuf *es_head;	/* first (oldest) request reference */
		struct pbuf **es_unsentp; /* ptr-ptr to first unsent request */
		struct pbuf **es_tailp;	/* ptr-ptr for adding new requests */
		unsigned int es_count;	/* buffer count, see ETHIF_PBUF_.. */
	} ethif_snd;
	struct {			/* receive queue (packets) */
		struct pbuf *er_head;	/* first (oldest) request buffer */
		struct pbuf **er_tailp;	/* ptr-ptr for adding new requests */
	} ethif_rcv;
	SIMPLEQ_ENTRY(ethif) ethif_next; /* next in free list */
} ethif_array[NR_NDEV];	/* any other value would be suboptimal */

#define ethif_get_name(ethif)	(ifdev_get_name(&(ethif)->ethif_ifdev))
#define ethif_get_netif(ethif)	(ifdev_get_netif(&(ethif)->ethif_ifdev))

#define ETHIFF_DISABLED		0x01	/* driver has disappeared */
#define ETHIFF_FIRST_CONF	0x02	/* first configuration request sent */

/*
 * Send queue limit settings.  Both are counted in number of pbuf objects.
 * ETHIF_PBUF_MIN is the minimum number of pbuf objects that can always be
 * enqueued on a particular interface's send queue.  It should be at least the
 * number of pbufs for one single packet after being reduced to the ndev limit,
 * so NDEV_IOV_MAX (8) is a natural fit.  The ETHIF_PBUF_MAX_n values define
 * the maximum number of pbufs that may be used by all interface send queues
 * combined, whichever of the two is smaller.  The resulting number must be set
 * fairly high, because at any time there may be a lot of active TCP sockets
 * that all generate a (multi-pbuf) packet as a result of a clock tick.  It is
 * currently a function of the size of the buffer pool, capped to a value that
 * is a function of the number of TCP sockets (assuming one packet per socket;
 * up to MSS/BUFSIZE+1 data pbufs, one header pbuf, one extra as margin).  The
 * difference between the per-interface guaranteed minimum and the global
 * maximum is what makes up a pool of "spares", which are really just tokens
 * allowing for enqueuing of that many pbufs.
 */
#define ETHIF_PBUF_MIN		(NDEV_IOV_MAX)
#define ETHIF_PBUF_MAX_1	(mempool_cur_buffers() >> 1)
#define ETHIF_PBUF_MAX_2	(NR_TCPSOCK * (TCP_MSS / MEMPOOL_BUFSIZE + 3))

static unsigned int ethif_spares;

static SIMPLEQ_HEAD(, ethif) ethif_freelist;	/* free ethif objects */

static const struct ifdev_ops ethif_ops;

#ifdef INET6
static ip6_addr_t ethif_ip6addr_allnodes_ll;
#endif /* INET6 */

/*
 * Initialize the ethernet interfaces module.
 */
void
ethif_init(void)
{
	unsigned int slot;

	/* Initialize the list of free ethif objects. */
	SIMPLEQ_INIT(&ethif_freelist);

	for (slot = 0; slot < __arraycount(ethif_array); slot++)
		SIMPLEQ_INSERT_TAIL(&ethif_freelist, &ethif_array[slot],
		    ethif_next);

	/* Initialize the number of in-use spare tokens. */
	ethif_spares = 0;

#ifdef INET6
	/* Preinitialize the link-local all-nodes IPv6 multicast address. */
	ip6_addr_set_allnodes_linklocal(&ethif_ip6addr_allnodes_ll);
#endif /* INET6 */
}

/*
 * As the result of some event, the NetBSD-style interface flags for this
 * interface may have changed.  Recompute and update the flags as appropriate.
 */
static void
ethif_update_ifflags(struct ethif * ethif)
{
	unsigned int ifflags;

	ifflags = ifdev_get_ifflags(&ethif->ethif_ifdev);

	/* These are the flags that we might update here. */
	ifflags &= ~(IFF_RUNNING | IFF_ALLMULTI);

	/*
	 * For us, the RUNNING flag indicates that -as far as we know- the
	 * network device is fully operational and has its I/O engines running.
	 * This is a reflection of the current state, not of any intention, so
	 * we look at the active configuration here.  We use the same approach
	 * for one other receive state flags here (ALLMULTI).
	 */
	if ((ethif->ethif_flags &
	    (ETHIFF_DISABLED | ETHIFF_FIRST_CONF)) == 0 &&
	     ethif->ethif_active.nconf_mode != NDEV_MODE_DOWN) {
		ifflags |= IFF_RUNNING;

		if (ethif->ethif_active.nconf_mode & NDEV_MODE_MCAST_ALL)
			ifflags |= IFF_ALLMULTI;
	}

	ifdev_update_ifflags(&ethif->ethif_ifdev, ifflags);
}

/*
 * Add a multicast hardware receive address into the set of hardware addresses
 * in the given configuration, if the given address is not already in the
 * configuration's set.  Adjust the configuration's mode as needed.  Return
 * TRUE If the address was added, and FALSE if the address could not be added
 * due to a full list (of 'max' elements), in which case the mode is changed
 * from receiving from listed multicast addresses to receiving from all
 * multicast addresses.
 */
static int
ethif_add_mcast(struct ndev_conf * nconf, unsigned int max,
	struct ndev_hwaddr * hwaddr)
{
	unsigned int slot;

	/*
	 * See if the hardware address is already in the list we produced so
	 * far.  This makes the multicast list generation O(n^2) but we do not
	 * expect many entries nor is the list size large anyway.
	 */
	for (slot = 0; slot < nconf->nconf_mccount; slot++)
		if (!memcmp(&nconf->nconf_mclist[slot], hwaddr,
		    sizeof(*hwaddr)))
			return TRUE;

	if (nconf->nconf_mccount < max) {
		memcpy(&nconf->nconf_mclist[slot], hwaddr, sizeof(*hwaddr));
		nconf->nconf_mccount++;

		nconf->nconf_mode |= NDEV_MODE_MCAST_LIST;

		return TRUE;
	} else {
		nconf->nconf_mode &= ~NDEV_MODE_MCAST_LIST;
		nconf->nconf_mode |= NDEV_MODE_MCAST_ALL;

		return FALSE;
	}
}

/*
 * Add the ethernet hardware address derived from the given IPv4 multicast
 * address, to the list of multicast addresses.
 */
static int
ethif_add_mcast_v4(struct ndev_conf * nconf, unsigned int max,
	const ip4_addr_t * ip4addr)
{
	struct ndev_hwaddr hwaddr;

	/* 01:00:05:xx:xx:xx with the lower 23 bits of the IPv4 address. */
	hwaddr.nhwa_addr[0] = LL_IP4_MULTICAST_ADDR_0;
	hwaddr.nhwa_addr[1] = LL_IP4_MULTICAST_ADDR_1;
	hwaddr.nhwa_addr[2] = LL_IP4_MULTICAST_ADDR_2;
	hwaddr.nhwa_addr[3] = (ip4_addr_get_u32(ip4addr) >> 16) & 0x7f;
	hwaddr.nhwa_addr[4] = (ip4_addr_get_u32(ip4addr) >>  8) & 0xff;
	hwaddr.nhwa_addr[5] = (ip4_addr_get_u32(ip4addr) >>  0) & 0xff;

	return ethif_add_mcast(nconf, max, &hwaddr);
}

/*
 * Add the ethernet hardware address derived from the given IPv6 multicast
 * address, to the list of multicast addresses.
 */
static int
ethif_add_mcast_v6(struct ndev_conf * nconf, unsigned int max,
	const ip6_addr_t * ip6addr)
{
	struct ndev_hwaddr hwaddr;

	/* 33:33:xx:xx:xx:xx with the lower 32 bits of the IPv6 address. */
	hwaddr.nhwa_addr[0] = LL_IP6_MULTICAST_ADDR_0;
	hwaddr.nhwa_addr[1] = LL_IP6_MULTICAST_ADDR_1;
	memcpy(&hwaddr.nhwa_addr[2], &ip6addr->addr[3], sizeof(uint32_t));

	return ethif_add_mcast(nconf, max, &hwaddr);
}

/*
 * Set up the multicast mode for a configuration that is to be sent to a
 * network driver, generating a multicast receive address list for the driver
 * as applicable.
 */
static void
ethif_gen_mcast(struct ethif * ethif, struct ndev_conf * nconf)
{
	struct igmp_group *group4;
	struct mld_group *group6;
	unsigned int max;

	/* Make sure that multicast is supported at all for this interface. */
	if (!(ethif->ethif_caps & NDEV_CAP_MCAST))
		return;

	/* Make sure the mode is being (re)configured to be up. */
	if (!(nconf->nconf_set & NDEV_SET_MODE) ||
	    nconf->nconf_mode == NDEV_MODE_DOWN)
		return;

	/* Recompute the desired multicast flags. */
	nconf->nconf_mode &= ~(NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL);

	/* If promiscuous mode is enabled, receive all multicast packets. */
	if (nconf->nconf_mode & NDEV_MODE_PROMISC) {
		nconf->nconf_mode |= NDEV_MODE_MCAST_ALL;

		return;
	}

	/*
	 * Map all IGMP/MLD6 multicast addresses to ethernet addresses, merging
	 * any duplicates to save slots.  We have to add the MLD6 all-nodes
	 * multicast address ourselves, which also means the list is never
	 * empty unless compiling with USE_INET6=no.  If the list is too small
	 * for all addresses, opt to receive all multicast packets instead.
	 */
	nconf->nconf_mclist = ethif->ethif_mclist;
	nconf->nconf_mccount = 0;
	max = __arraycount(ethif->ethif_mclist);

	for (group4 = netif_igmp_data(ethif_get_netif(ethif)); group4 != NULL;
	    group4 = group4->next)
		if (!ethif_add_mcast_v4(nconf, max, &group4->group_address))
			return;

#ifdef INET6
	if (!ethif_add_mcast_v6(nconf, max, &ethif_ip6addr_allnodes_ll))
		return;
#endif /* INET6 */

	for (group6 = netif_mld6_data(ethif_get_netif(ethif)); group6 != NULL;
	    group6 = group6->next)
		if (!ethif_add_mcast_v6(nconf, max, &group6->group_address))
			return;
}

/*
 * Merge a source configuration into a destination configuration, copying any
 * fields intended to be set from the source into the destination and clearing
 * the "set" mask in the source, without changing the source fields, so that
 * the source will reflect the destination's contents.
 */
static void
ethif_merge_conf(struct ndev_conf * dconf, struct ndev_conf * sconf)
{

	dconf->nconf_set |= sconf->nconf_set;

	if (sconf->nconf_set & NDEV_SET_MODE)
		dconf->nconf_mode = sconf->nconf_mode;
	if (sconf->nconf_set & NDEV_SET_CAPS)
		dconf->nconf_caps = sconf->nconf_caps;
	if (sconf->nconf_set & NDEV_SET_FLAGS)
		dconf->nconf_flags = sconf->nconf_flags;
	if (sconf->nconf_set & NDEV_SET_MEDIA)
		dconf->nconf_media = sconf->nconf_media;
	if (sconf->nconf_set & NDEV_SET_HWADDR)
		memcpy(&dconf->nconf_hwaddr, &sconf->nconf_hwaddr,
		    sizeof(dconf->nconf_hwaddr));

	sconf->nconf_set = 0;
}

/*
 * Return TRUE if we can and should try to pass a configuration request to the
 * ndev layer on this interface, or FALSE otherwise.
 */
static int
ethif_can_conf(struct ethif * ethif)
{

	/* Is there a configuration change waiting?  The common case is no. */
	if (ethif->ethif_wanted.nconf_set == 0)
		return FALSE;

	/*
	 * Is there a configuration change pending already?  Then wait for it
	 * to be acknowledged first.
	 */
	if (ethif->ethif_pending.nconf_set != 0)
		return FALSE;

	/* Make sure the interface is in the appropriate state. */
	if (ethif->ethif_flags & ETHIFF_DISABLED)
		return FALSE;

	/* First let all current packet send requests finish. */
	return (ethif->ethif_snd.es_unsentp == &ethif->ethif_snd.es_head);
}

/*
 * Return TRUE if we can and should try to pass the next unsent packet send
 * request to the ndev layer on this interface, or FALSE otherwise.
 */
static int
ethif_can_send(struct ethif * ethif)
{

	/* Is there anything to hand to ndev at all?  The common case is no. */
	if (*ethif->ethif_snd.es_unsentp == NULL)
		return FALSE;

	/*
	 * Is there a configuration change pending?  Then we cannot send
	 * packets yet.  Always let all configuration changes through first.
	 */
	if (ethif->ethif_pending.nconf_set != 0 ||
	    ethif->ethif_wanted.nconf_set != 0)
		return FALSE;

	/* Make sure the interface is in the appropriate state. */
	if ((ethif->ethif_flags & (ETHIFF_DISABLED | ETHIFF_FIRST_CONF)) != 0)
		return FALSE;

	return TRUE;
}

/*
 * Return TRUE if we can and should try to receive packets on this interface
 * and are ready to accept received packets, or FALSE otherwise.
 */
static int
ethif_can_recv(struct ethif * ethif)
{

	if ((ethif->ethif_flags & (ETHIFF_DISABLED | ETHIFF_FIRST_CONF)) != 0)
		return FALSE;

	/*
	 * We do not check the link status here.  There is no reason not to
	 * spawn receive requests, or accept received packets, while the link
	 * is reported to be down.
	 */
	return ifdev_is_up(&ethif->ethif_ifdev);
}

/*
 * Polling function, invoked after each message loop iteration.  Check whether
 * any configuration change or packets can be sent to the driver, and whether
 * any new packet receive requests can be enqueued at the driver.
 */
static void
ethif_poll(struct ifdev * ifdev)
{
	struct ethif *ethif = (struct ethif *)ifdev;
	struct pbuf *pbuf, *pref;

	/*
	 * If a configuration request is desired, see if we can send it to the
	 * driver now.  Otherwise, attempt to send any packets if possible.
	 * In both cases, a failure of the ndev call indicates that we should
	 * try again later.
	 */
	if (ethif_can_conf(ethif)) {
		ethif_gen_mcast(ethif, &ethif->ethif_wanted);

		/*
		 * On success, move the wanted configuration into the pending
		 * slot.  Otherwise, try again on the next poll iteration.
		 */
		if (ndev_conf(ethif->ethif_ndev, &ethif->ethif_wanted) == OK)
			ethif_merge_conf(&ethif->ethif_pending,
			    &ethif->ethif_wanted);
	} else {
		while (ethif_can_send(ethif)) {
			pref = *ethif->ethif_snd.es_unsentp;

			if (pref->type == PBUF_REF)
				pbuf = (struct pbuf *)pref->payload;
			else
				pbuf = pref;

			if (ndev_send(ethif->ethif_ndev, pbuf) == OK)
				ethif->ethif_snd.es_unsentp =
				    pchain_end(pref);
			else
				break;
		}
	}

	/*
	 * Attempt to create additional receive requests for the driver, if
	 * applicable.  We currently do not set a limit on the maximum number
	 * of concurrently pending receive requests here, because the maximum
	 * in ndev is already quite low.  That may have to be changed one day.
	 */
	while (ethif_can_recv(ethif) && ndev_can_recv(ethif->ethif_ndev)) {
		/*
		 * Allocate a buffer for the network device driver to copy the
		 * received packet into.  Allocation may fail if no buffers are
		 * available at this time; in that case simply try again later.
		 * We add room for a VLAN tag even though we do not support
		 * such tags just yet.
		 */
		if ((pbuf = pchain_alloc(PBUF_RAW, ETH_PAD_LEN + ETH_HDR_LEN +
		    ETHIF_MAX_MTU + NDEV_ETH_PACKET_TAG)) == NULL)
			break;

		/*
		 * Effectively throw away two bytes in order to align TCP/IP
		 * header fields to 32 bits.  See the short discussion in
		 * lwipopts.h as to why we are not using lwIP's ETH_PAD_SIZE.
		 */
		util_pbuf_header(pbuf, -ETH_PAD_LEN);

		/*
		 * Send the request to the driver.  This may still fail due to
		 * grant allocation failure, in which case we try again later.
		 */
		if (ndev_recv(ethif->ethif_ndev, pbuf) != OK) {
			pbuf_free(pbuf);

			break;
		}

		/*
		 * Hold on to the packet buffer until the receive request
		 * completes or is aborted, or the driver disappears.
		 */
		*ethif->ethif_rcv.er_tailp = pbuf;
		ethif->ethif_rcv.er_tailp = pchain_end(pbuf);
	}
}

/*
 * Complete the link-layer header of the packet by filling in a source address.
 * This is relevant for BPF-generated packets only, and thus we can safely
 * modify the given pbuf.
 */
static void
ethif_hdrcmplt(struct ifdev * ifdev, struct pbuf * pbuf)
{
	struct netif *netif;

	/* Make sure there is an ethernet packet header at all. */
	if (pbuf->len < ETH_HDR_LEN)
		return;

	netif = ifdev_get_netif(ifdev);

	/*
	 * Insert the source ethernet address into the packet.  The source
	 * address is located right after the destination address at the start
	 * of the packet.
	 */
	memcpy((uint8_t *)pbuf->payload + netif->hwaddr_len, netif->hwaddr,
	    netif->hwaddr_len);
}

/*
 * Return TRUE if the given additional number of spare tokens may be used, or
 * FALSE if the limit has been reached.  Each spare token represents one
 * enqueued pbuf.  The limit must be such that we do not impede normal traffic
 * but also do not spend the entire buffer pool on enqueued packets.
 */
static int
ethif_can_spare(unsigned int spares)
{
	unsigned int max;

	/*
	 * Use the configured maximum, which depends on the current size of the
	 * buffer pool.
	 */
	max = ETHIF_PBUF_MAX_1;

	/*
	 * However, limit the total to a value based on the maximum number of
	 * TCP packets that can, in the worst case, be expected to queue up at
	 * any single moment.
	 */
	if (max > ETHIF_PBUF_MAX_2)
		max = ETHIF_PBUF_MAX_2;

	return (spares + ethif_spares <= max - ETHIF_PBUF_MIN * NR_NDEV);
}

/*
 * Process a packet as output on an ethernet interface.
 */
static err_t
ethif_output(struct ifdev * ifdev, struct pbuf * pbuf, struct netif * netif)
{
	struct ethif *ethif = (struct ethif *)ifdev;
	struct pbuf *pref, *pcopy;
	size_t padding;
	unsigned int count, spares;

	/* Packets must never be sent on behalf of another interface. */
	assert(netif == NULL);

	/*
	 * The caller already rejects packets while the interface or link is
	 * down.  We do want to keep enqueuing packets while the driver is
	 * restarting, so do not check ETHIFF_DISABLED or ETHIFF_FIRST_CONF.
	 */

	/*
	 * Reject oversized packets immediately.  This should not happen.
	 * Undersized packets are padded below.
	 */
	if (pbuf->tot_len > NDEV_ETH_PACKET_MAX) {
		printf("LWIP: attempt to send oversized ethernet packet "
		    "(size %u)\n", pbuf->tot_len);
		util_stacktrace();

		return ERR_MEM;
	}

	/*
	 * The original lwIP idea for processing output packets is that we make
	 * a copy of the packet here, so that lwIP is free to do whatever it
	 * wants with the original packet (e.g., keep on the TCP retransmission
	 * queue).  More recently, lwIP has made progress towards allowing the
	 * packet to be referenced only, decreasing the reference count only
	 * once the packet has been actually sent.  For many embedded systems,
	 * that change now allows zero-copy transmission with direct DMA from
	 * the provided packet buffer.  We are not so lucky: we have to make an
	 * additional inter-process copy anyway.  We do however use the same
	 * referencing system to avoid having to make yet another copy of the
	 * packet here.
	 *
	 * There was previously a check on (pbuf->ref > 1) here, to ensure that
	 * we would never enqueue packets that are retransmitted while we were
	 * still in the process of sending the initial copy.  Now that for ARP
	 * and NDP queuing, packets are referenced rather than copied (lwIP
	 * patch #9272), we can no longer perform that check: packets may
	 * legitimately have a reference count of 2 at this point.  The second
	 * reference will be dropped by the caller immediately after we return.
	 */

	/*
	 * There are two cases in which we need to make a copy of the packet
	 * after all:
	 *
	 * 1) in the case that the packet needs to be padded in order to reach
	 *    the minimum ethernet packet size (for drivers' convenience);
	 * 2) in the (much more exceptional) case that the given pbuf chain
	 *    exceeds the maximum vector size for network driver requests.
	 */
	if (NDEV_ETH_PACKET_MIN > pbuf->tot_len)
		padding = NDEV_ETH_PACKET_MIN - pbuf->tot_len;
	else
		padding = 0;

	count = pbuf_clen(pbuf);

	if (padding != 0 || count > NDEV_IOV_MAX) {
		pcopy = pchain_alloc(PBUF_RAW, pbuf->tot_len + padding);
		if (pcopy == NULL) {
			ifdev_output_drop(ifdev);

			return ERR_MEM;
		}

		if (pbuf_copy(pcopy, pbuf) != ERR_OK)
			panic("unexpected pbuf copy failure");

		if (padding > 0) {
			/*
			 * This restriction can be lifted if needed, but it
			 * involves hairy pbuf traversal and our standard pool
			 * size should be way in excess of the minimum packet
			 * size.
			 */
			assert(pcopy->len == pbuf->tot_len + padding);

			memset((char *)pcopy->payload + pbuf->tot_len, 0,
			    padding);
		}

		count = pbuf_clen(pcopy);
		assert(count <= NDEV_IOV_MAX);

		pbuf = pcopy;
	} else
		pcopy = NULL;

	/*
	 * Restrict the size of the send queue, so that it will not exhaust the
	 * buffer pool.
	 */
	if (ethif->ethif_snd.es_count >= ETHIF_PBUF_MIN)
		spares = count;
	else if (ethif->ethif_snd.es_count + count > ETHIF_PBUF_MIN)
		spares = ethif->ethif_snd.es_count + count - ETHIF_PBUF_MIN;
	else
		spares = 0;

	if (spares > 0 && !ethif_can_spare(spares)) {
		if (pcopy != NULL)
			pbuf_free(pcopy);

		ifdev_output_drop(ifdev);

		return ERR_MEM;
	}

	/*
	 * A side effect of the referencing approach is that we cannot touch
	 * the last pbuf's "next" pointer.  Thus, we need another way of
	 * linking together the buffers on the send queue.  We use a linked
	 * list of PBUF_REF-type buffers for this instead.  However, do this
	 * only when we have not made a copy of the original pbuf, because then
	 * we might as well use the copy instead.
	 */
	if (pcopy == NULL) {
		if ((pref = pbuf_alloc(PBUF_RAW, 0, PBUF_REF)) == NULL) {
			ifdev_output_drop(ifdev);

			return ERR_MEM;
		}

		pbuf_ref(pbuf);

		pref->payload = pbuf;
		pref->tot_len = 0;
		pref->len = count;
	} else
		pref = pcopy;

	/* If the send queue was empty so far, set the IFF_OACTIVE flag. */
	if (ethif->ethif_snd.es_head == NULL)
		ifdev_update_ifflags(&ethif->ethif_ifdev,
		    ifdev_get_ifflags(&ethif->ethif_ifdev) | IFF_OACTIVE);

	/*
	 * Enqueue the packet on the send queue.  It will be sent from the
	 * polling function as soon as possible.  TODO: see if sending it from
	 * here makes any performance difference at all.
	 */
	*ethif->ethif_snd.es_tailp = pref;
	ethif->ethif_snd.es_tailp = pchain_end(pref);

	ethif->ethif_snd.es_count += count;
	ethif_spares += spares;

	return ERR_OK;
}

/*
 * Transmit an ethernet packet on an ethernet interface, as requested by lwIP.
 */
static err_t
ethif_linkoutput(struct netif * netif, struct pbuf * pbuf)
{
	struct ifdev *ifdev = netif_get_ifdev(netif);

	/*
	 * Let ifdev make the callback to our output function, so that it can
	 * pass the packet to BPF devices and generically update statistics.
	 */
	return ifdev_output(ifdev, pbuf, NULL /*netif*/, TRUE /*to_bpf*/,
	    TRUE /*hdrcmplt*/);
}

/*
 * The multicast address list has changed.  See to it that the change will make
 * it to the network driver at some point.
 */
static err_t
ethif_set_mcast(struct ethif * ethif)
{

	/*
	 * Simply generate a mode change request, unless the interface is down.
	 * Once the mode change request is about to be sent to the driver, we
	 * will recompute the multicast settings.
	 */
	if (ifdev_is_up(&ethif->ethif_ifdev))
		ethif->ethif_wanted.nconf_set |= NDEV_SET_MODE;

	return ERR_OK;
}

/*
 * An IPv4 multicast address has been added to or removed from the list of IPv4
 * multicast addresses.
 */
static err_t
ethif_set_mcast_v4(struct netif * netif, const ip4_addr_t * group __unused,
	enum netif_mac_filter_action action __unused)
{

	return ethif_set_mcast((struct ethif *)netif_get_ifdev(netif));
}

/*
 * An IPv6 multicast address has been added to or removed from the list of IPv6
 * multicast addresses.
 */
static err_t
ethif_set_mcast_v6(struct netif * netif, const ip6_addr_t * group __unused,
	enum netif_mac_filter_action action __unused)
{

	return ethif_set_mcast((struct ethif *)netif_get_ifdev(netif));
}

/*
 * Initialization function for an ethernet-type netif interface, called from
 * lwIP at interface creation time.
 */
static err_t
ethif_init_netif(struct ifdev * ifdev, struct netif * netif)
{
	struct ethif *ethif = (struct ethif *)ifdev;

	/*
	 * Fill in a dummy name.  Since it is only two characters, do not
	 * bother trying to reuse part of the given name.  If this name is ever
	 * actually used anywhere, the dummy should suffice for debugging.
	 */
	netif->name[0] = 'e';
	netif->name[1] = 'n';

	netif->linkoutput = ethif_linkoutput;

	memset(netif->hwaddr, 0, sizeof(netif->hwaddr));

	/*
	 * Set the netif flags, partially based on the capabilities reported by
	 * the network device driver.  The reason that we do this now is that
	 * lwIP tests for some of these flags and starts appropriate submodules
	 * (e.g., IGMP) right after returning from this function.  If we set
	 * the flags later, we also have to take over management of those
	 * submodules, which is something we'd rather avoid.  For this reason
	 * in particular, we also do not support capability mask changes after
	 * driver restarts - see ethif_enable().
	 */
	netif->flags = NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

	if (ethif->ethif_caps & NDEV_CAP_BCAST)
		netif->flags |= NETIF_FLAG_BROADCAST;

	if (ethif->ethif_caps & NDEV_CAP_MCAST) {
		/* The IGMP code adds the all-stations multicast entry. */
		netif->igmp_mac_filter = ethif_set_mcast_v4;

		netif->flags |= NETIF_FLAG_IGMP;

		/* For MLD6 we have to add the all-nodes entry ourselves. */
		netif->mld_mac_filter = ethif_set_mcast_v6;

		netif->flags |= NETIF_FLAG_MLD6;
	}

	return ERR_OK;
}

/*
 * The ndev layer reports that a new network device driver has appeared, with
 * the given ndev identifier, a driver-given name, and a certain set of
 * capabilities.  Create a new ethernet interface object for it.  On success,
 * return a pointer to the object (for later callbacks from ndev).  In that
 * case, the ndev layer will always immediately call ethif_enable() afterwards.
 * On failure, return NULL, in which case ndev will forget about the driver.
 */
struct ethif *
ethif_add(ndev_id_t id, const char * name, uint32_t caps)
{
	struct ethif *ethif;
	unsigned int ifflags;
	int r;

	/*
	 * First make sure that the interface name is valid, unique, and not
	 * reserved for virtual interface types.
	 */
	if ((r = ifdev_check_name(name, NULL /*vtype_slot*/)) != OK) {
		/*
		 * There is some risk in printing bad stuff, but this may help
		 * in preventing serious driver writer frustration..
		 */
		printf("LWIP: invalid driver name '%s' (%d)\n", name, r);

		return NULL;
	}

	/* Then see if there is a free ethernet interface object available. */
	if (SIMPLEQ_EMPTY(&ethif_freelist)) {
		printf("LWIP: out of slots for driver name '%s'\n", name);

		return NULL;
	}

	/*
	 * All good; set up the interface.  First initialize the object, since
	 * adding the interface to lwIP might spawn some activity right away.
	 */
	ethif = SIMPLEQ_FIRST(&ethif_freelist);
	SIMPLEQ_REMOVE_HEAD(&ethif_freelist, ethif_next);

	/* Initialize the ethif structure. */
	memset(ethif, 0, sizeof(*ethif));
	ethif->ethif_ndev = id;
	ethif->ethif_flags = ETHIFF_DISABLED;
	ethif->ethif_caps = caps;

	ethif->ethif_snd.es_head = NULL;
	ethif->ethif_snd.es_unsentp = &ethif->ethif_snd.es_head;
	ethif->ethif_snd.es_tailp = &ethif->ethif_snd.es_head;
	ethif->ethif_snd.es_count = 0;

	ethif->ethif_rcv.er_head = NULL;
	ethif->ethif_rcv.er_tailp = &ethif->ethif_rcv.er_head;

	/*
	 * Set all the three configurations to the same initial values.  Since
	 * any change to the configuration will go through all three, this
	 * allows us to obtain various parts of the status (in particular, the
	 * mode, flags, enabled capabilities, and media type selection) from
	 * any of the three without having to consult the others.  Note that
	 * the hardware address is set to a indeterminate initial value, as it
	 * is left to the network driver unless specifically overridden.
	 */
	ethif->ethif_active.nconf_set = 0;
	ethif->ethif_active.nconf_mode = NDEV_MODE_DOWN;
	ethif->ethif_active.nconf_flags = 0;
	ethif->ethif_active.nconf_caps = 0;
	ethif->ethif_active.nconf_media =
	    IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0);
	memcpy(&ethif->ethif_pending, &ethif->ethif_active,
	    sizeof(ethif->ethif_pending));
	memcpy(&ethif->ethif_wanted, &ethif->ethif_pending,
	    sizeof(ethif->ethif_wanted));

	/*
	 * Compute the initial NetBSD-style interface flags.  The IFF_SIMPLEX
	 * interface flag is always enabled because we do not support network
	 * drivers that are receiving their own packets.  In particular, lwIP
	 * currently does not deal well with receiving back its own multicast
	 * packets, which leads to IPv6 DAD failures.  The other two flags
	 * (IFF_BROADCAST, IFF_MULTICAST) denote capabilities, not enabled
	 * receipt modes.
	 */
	ifflags = IFF_SIMPLEX;
	if (caps & NDEV_CAP_BCAST)
		ifflags |= IFF_BROADCAST;
	if (caps & NDEV_CAP_MCAST)
		ifflags |= IFF_MULTICAST;

	/* Finally, add the interface to ifdev and lwIP.  This cannot fail. */
	ifdev_add(&ethif->ethif_ifdev, name, ifflags, IFT_ETHER, ETH_HDR_LEN,
	    ETHARP_HWADDR_LEN, DLT_EN10MB, ETHIF_DEF_MTU,
	    ND6_IFF_PERFORMNUD | ND6_IFF_AUTO_LINKLOCAL, &ethif_ops);

	return ethif;
}

/*
 * The link status and/or media type of an ethernet interface has changed.
 */
static void
ethif_set_status(struct ethif * ethif, uint32_t link, uint32_t media)
{
	unsigned int iflink;

	/* We save the media type locally for now. */
	ethif->ethif_media = media;

	/* Let the ifdev module handle the details of the link change. */
	switch (link) {
	case NDEV_LINK_UP:	iflink = LINK_STATE_UP;		break;
	case NDEV_LINK_DOWN:	iflink = LINK_STATE_DOWN;	break;
	default:		iflink = LINK_STATE_UNKNOWN;	break;
	}

	ifdev_update_link(&ethif->ethif_ifdev, iflink);
}

/*
 * The ndev layer reports that a previously added or disabled network device
 * driver has been (re)enabled.  Start by initializing the driver.  Return TRUE
 * if the interface could indeed be enabled, or FALSE if it should be forgotten
 * altogether after all.
 */
int
ethif_enable(struct ethif * ethif, const char * name,
	const struct ndev_hwaddr * hwaddr, uint8_t hwaddr_len, uint32_t caps,
	uint32_t link, uint32_t media)
{
	int r;

	assert(ethif->ethif_flags & ETHIFF_DISABLED);

	/*
	 * One disadvantage of keeping service labels and ethernet driver names
	 * disjunct is that the ethernet driver may mess with its name between
	 * restarts.  Ultimately we may end up renaming our ethernet drivers
	 * such that their labels match their names, in which case we no longer
	 * need the drivers themselves to produce a name, and we can retire
	 * this check.
	 */
	if (name != NULL && strcmp(ethif_get_name(ethif), name)) {
		printf("LWIP: driver '%s' restarted with name '%s'\n",
		    ethif_get_name(ethif), name);

		return FALSE;
	}

	/*
	 * The hardware address length is just a sanity check for now.  After
	 * the initialization reply, we assume the same length is used for all
	 * addresses, which is also the maximum, namely 48 bits (six bytes).
	 */
	if (hwaddr_len != ETHARP_HWADDR_LEN) {
		printf("LWIP: driver '%s' reports hwaddr length %u\n",
		    ethif_get_name(ethif), hwaddr_len);

		return FALSE;
	}

	/*
	 * If the driver has changed its available capabilities as a result of
	 * a restart, we have a problem: we may already have configured the
	 * interface's netif object to make use of of some of those
	 * capabilities.  TODO: we can deal with some cases (e.g., disappearing
	 * checksum offloading capabilities) with some effort, and with other
	 * cases (e.g., disappearing multicast support) with a LOT more effort.
	 */
	if (ethif->ethif_caps != caps) {
		printf("LWIP: driver '%s' changed capabilities\n",
		    ethif_get_name(ethif));

		return FALSE;
	}

	/*
	 * Set the hardware address on the interface, unless a request is
	 * currently pending to change it, in which case the new address has
	 * been set already and we do not want to revert that change.  If not,
	 * we always set the address, because it may have changed as part of a
	 * driver restart and we do not want to get out of sync with it, nor
	 * can we necessarily change it back.
	 */
	if (!(ethif->ethif_active.nconf_set & NDEV_SET_HWADDR) &&
	    !(ethif->ethif_pending.nconf_set & NDEV_SET_HWADDR))
		ifdev_update_hwaddr(&ethif->ethif_ifdev, hwaddr->nhwa_addr,
		    (name == NULL) /*is_factory*/);

	/*
	 * At this point, only one more thing can fail: it is possible that we
	 * do not manage to send the first configuration request due to memory
	 * shortage.  This is extremely unlikely to happen, so send the conf
	 * request first and forget the entire driver if it fails.
	 */
	/*
	 * Always generate a new multicast list before sending a configuration
	 * request, and at no other time (since there may be a grant for it).
	 */
	ethif_gen_mcast(ethif, &ethif->ethif_active);

	if ((r = ndev_conf(ethif->ethif_ndev, &ethif->ethif_active)) != OK) {
		printf("LWIP: sending first configuration to '%s' failed "
		    "(%d)\n", ethif_get_name(ethif), r);

		return FALSE;
	}

	ethif_set_status(ethif, link, media);

	ethif->ethif_flags &= ~ETHIFF_DISABLED;
	ethif->ethif_flags |= ETHIFF_FIRST_CONF;

	return TRUE;
}

/*
 * The configuration change stored in the "pending" slot of the given ethif
 * object has been acknowledged by the network device driver (or the driver has
 * died, see ethif_disable()).  Apply changes to the "active" slot of the given
 * ethif object, as well as previously delayed changes to lwIP through netif.
 */
static void
ethif_post_conf(struct ethif * ethif)
{
	struct ndev_conf *nconf;
	unsigned int flags;

	nconf = &ethif->ethif_pending;

	/*
	 * Now that the driver configuration has changed, we know that the
	 * new checksum settings will be applied to all sent and received
	 * packets, and we can disable checksumming flags in netif as desired.
	 * Enabling checksumming flags has already been done earlier on.
	 */
	if (nconf->nconf_set & NDEV_SET_CAPS) {
		flags = ethif_get_netif(ethif)->chksum_flags;

		if (nconf->nconf_caps & NDEV_CAP_CS_IP4_TX)
			flags &= ~NETIF_CHECKSUM_GEN_IP;
		if (nconf->nconf_caps & NDEV_CAP_CS_IP4_RX)
			flags &= ~NETIF_CHECKSUM_CHECK_IP;
		if (nconf->nconf_caps & NDEV_CAP_CS_UDP_TX)
			flags &= ~NETIF_CHECKSUM_GEN_UDP;
		if (nconf->nconf_caps & NDEV_CAP_CS_UDP_RX)
			flags &= ~NETIF_CHECKSUM_CHECK_UDP;
		if (nconf->nconf_caps & NDEV_CAP_CS_TCP_TX)
			flags &= ~NETIF_CHECKSUM_GEN_TCP;
		if (nconf->nconf_caps & NDEV_CAP_CS_TCP_RX)
			flags &= ~NETIF_CHECKSUM_CHECK_TCP;

		NETIF_SET_CHECKSUM_CTRL(ethif_get_netif(ethif), flags);
	}

	/*
	 * Merge any individual parts of the now acknowledged configuration
	 * changes into the active configuration.  The result is that we are
	 * able to reapply these changes at any time should the network driver
	 * be restarted.  In addition, by only setting bits for fields that
	 * have actually changed, we can later tell whether the user wanted the
	 * change or ethif should just take over what the driver reports after
	 * a restart; this is important for HW-address and media settings.
	 */
	ethif_merge_conf(&ethif->ethif_active, &ethif->ethif_pending);
}

/*
 * All receive requests have been canceled at the ndev layer, because the
 * network device driver has been restarted or shut down.  Clear the receive
 * queue, freeing any packets in it.
 */
static void
ethif_drain(struct ethif * ethif)
{
	struct pbuf *pbuf, **pnext;

	while ((pbuf = ethif->ethif_rcv.er_head) != NULL) {
		pnext = pchain_end(pbuf);

		if ((ethif->ethif_rcv.er_head = *pnext) == NULL)
			ethif->ethif_rcv.er_tailp = &ethif->ethif_rcv.er_head;

		*pnext = NULL;
		pbuf_free(pbuf);
	}
}

/*
 * The network device driver has stopped working (i.e., crashed), but has not
 * been shut down completely, and is expect to come back later.
 */
void
ethif_disable(struct ethif * ethif)
{

	/*
	 * We assume, optimistically, that a new instance of the driver will be
	 * brought up soon after which we can continue operating as before.  As
	 * such, we do not want to change most of the user-visible state until
	 * we know for sure that our optimism was in vain.  In particular, we
	 * do *not* want to change the following parts of the state here:
	 *
	 *   - the contents of the send queue;
	 *   - the state of the interface (up or down);
	 *   - the state and media type of the physical link.
	 *
	 * The main user-visible indication of the crash will be that the
	 * interface does not have the IFF_RUNNING flag set.
	 */

	/*
	 * If a configuration request was pending, it will be lost now.  Highly
	 * unintuitively, make the requested configuration the *active* one,
	 * just as though the request completed successfully.  This works,
	 * because once the driver comes back, the active configuration will be
	 * replayed as initial configuration.  Therefore, by pretending that
	 * the current request went through, we ensure that it too will be sent
	 * to the new instance--before anything else is allowed to happen.
	 */
	if (ethif->ethif_pending.nconf_set != 0)
		ethif_post_conf(ethif);

	/*
	 * Any packet send requests have been lost, too, and likewise forgotten
	 * by ndev.  Thus, we need to forget that we sent any packets, so that
	 * they will be resent after the driver comes back up.  That *may*
	 * cause packet duplication, but that is preferable over packet loss.
	 */
	ethif->ethif_snd.es_unsentp = &ethif->ethif_snd.es_head;

	/*
	 * We fully restart the receive queue, because all receive requests
	 * have been forgotten by ndev as well now and it is easier to simply
	 * reconstruct the receive queue in its entirety later on.
	 */
	ethif_drain(ethif);

	/* Make sure we do not attempt to initiate new requests for now. */
	ethif->ethif_flags &= ~ETHIFF_FIRST_CONF;
	ethif->ethif_flags |= ETHIFF_DISABLED;
}

/*
 * Dequeue and discard the packet at the head of the send queue.
 */
static void
ethif_dequeue_send(struct ethif * ethif)
{
	struct pbuf *pref, *pbuf, **pnext;
	unsigned int count, spares;

	/*
	 * The send queue is a linked list of reference buffers, each of which
	 * links to the actual packet.  Dequeue the first reference buffer.
	 */
	pref = ethif->ethif_snd.es_head;
	assert(pref != NULL);

	pnext = pchain_end(pref);

	if (ethif->ethif_snd.es_unsentp == pnext)
		ethif->ethif_snd.es_unsentp = &ethif->ethif_snd.es_head;

	if ((ethif->ethif_snd.es_head = *pnext) == NULL)
		ethif->ethif_snd.es_tailp = &ethif->ethif_snd.es_head;

	/* Do this before possibly calling pbuf_clen() below.. */
	*pnext = NULL;

	/*
	 * If we never made a copy of the original packet, we now have it
	 * pointed to by a reference buffer.  If so, decrease the reference
	 * count of the actual packet, thereby freeing it if lwIP itself was
	 * already done with.  Otherwise, the copy of the packet is the
	 * reference buffer itself.  In both cases we need to free that buffer.
	 */
	if (pref->type == PBUF_REF) {
		pbuf = (struct pbuf *)pref->payload;

		pbuf_free(pbuf);

		count = pref->len;
	} else
		count = pbuf_clen(pref);

	assert(count > 0);
	assert(ethif->ethif_snd.es_count >= count);
	ethif->ethif_snd.es_count -= count;

	if (ethif->ethif_snd.es_count >= ETHIF_PBUF_MIN)
		spares = count;
	else if (ethif->ethif_snd.es_count + count > ETHIF_PBUF_MIN)
		spares = ethif->ethif_snd.es_count + count - ETHIF_PBUF_MIN;
	else
		spares = 0;

	assert(ethif_spares >= spares);
	ethif_spares -= spares;

	/* Free the reference buffer as well. */
	pbuf_free(pref);

	/* If the send queue is now empty, clear the IFF_OACTIVE flag. */
	if (ethif->ethif_snd.es_head == NULL)
		ifdev_update_ifflags(&ethif->ethif_ifdev,
		    ifdev_get_ifflags(&ethif->ethif_ifdev) & ~IFF_OACTIVE);
}

/*
 * The ndev layer reports that a network device driver has been permanently
 * shut down.  Remove the corresponding ethernet interface from the system.
 */
void
ethif_remove(struct ethif * ethif)
{
	int r;

	/* Clear the send and receive queues. */
	while (ethif->ethif_snd.es_head != NULL)
		ethif_dequeue_send(ethif);

	ethif_drain(ethif);

	/* Let the ifdev module deal with most other removal aspects. */
	if ((r = ifdev_remove(&ethif->ethif_ifdev)) != OK)
		panic("unable to remove ethernet interface: %d", r);

	/* Finally, readd the ethif object to the free list. */
	SIMPLEQ_INSERT_HEAD(&ethif_freelist, ethif, ethif_next);
}

/*
 * The ndev layer reports that the (oldest) pending configuration request has
 * completed with the given result.
 */
void
ethif_configured(struct ethif * ethif, int32_t result)
{

	/*
	 * The driver is not supposed to return failure in response to a
	 * configure result.  If it does, we have no proper way to recover, as
	 * we may already have applied part of the new configuration to netif.
	 * For now, just report failure and then pretend success.
	 */
	if (result < 0) {
		printf("LWIP: driver '%s' replied with conf result %d\n",
		    ethif_get_name(ethif), result);

		result = 0;
	}

	if (ethif->ethif_flags & ETHIFF_FIRST_CONF)
		ethif->ethif_flags &= ~ETHIFF_FIRST_CONF;
	else
		ethif_post_conf(ethif);

	/*
	 * For now, the result is simply a boolean value indicating whether the
	 * driver is using the all-multicast receive mode instead of the
	 * multicast-list receive mode.  We can turn it into a bitmap later.
	 */
	if (result != 0) {
		ethif->ethif_active.nconf_mode &= ~NDEV_MODE_MCAST_LIST;
		ethif->ethif_active.nconf_mode |= NDEV_MODE_MCAST_ALL;
	}

	/* The interface flags may have changed now, so update them. */
	ethif_update_ifflags(ethif);

	/* Regular operation will resume from the polling function. */
}

/*
 * The ndev layer reports that the first packet on the send queue has been
 * successfully transmitted with 'result' set to OK, or dropped if 'result' is
 * negative.  The latter may happen if the interface was taken down while there
 * were still packets in transit.
 */
void
ethif_sent(struct ethif * ethif, int32_t result)
{

	ethif_dequeue_send(ethif);

	if (result < 0)
		ifdev_output_drop(&ethif->ethif_ifdev);

	/* More requests may be sent from the polling function now. */
}

/*
 * The ndev layer reports that the first buffer on the receive queue has been
 * filled with a packet of 'result' bytes, or if 'result' is negative, the
 * receive request has been aborted.
 */
void
ethif_received(struct ethif * ethif, int32_t result)
{
	struct pbuf *pbuf, *pwalk, **pnext;
	size_t left;

	/*
	 * Start by removing the first buffer chain off the receive queue.  The
	 * ndev layer guarantees that there ever was a receive request at all.
	 */
	if ((pbuf = ethif->ethif_rcv.er_head) == NULL)
		panic("driver received packet but queue empty");

	pnext = pchain_end(pbuf);

	if ((ethif->ethif_rcv.er_head = *pnext) == NULL)
		ethif->ethif_rcv.er_tailp = &ethif->ethif_rcv.er_head;
	*pnext = NULL;

	/* Decide if we can and should deliver a packet to the layers above. */
	if (result <= 0 || !ethif_can_recv(ethif)) {
		pbuf_free(pbuf);

		return;
	}

	if (result > pbuf->tot_len) {
		printf("LWIP: driver '%s' returned bad packet size (%zd)\n",
		    ethif_get_name(ethif), (ssize_t)result);

		pbuf_free(pbuf);

		return;
	}

	/*
	 * The packet often does not use all of the buffers, or at least not
	 * all of the last buffer.  Adjust lengths for the buffers that contain
	 * part of the packet, and free the remaining (unused) buffers, if any.
	 */
	left = (size_t)result;

	for (pwalk = pbuf; ; pwalk = pwalk->next) {
		pwalk->tot_len = left;
		if (pwalk->len > left)
			pwalk->len = left;
		left -= pwalk->len;
		if (left == 0)
			break;
	}

	if (pwalk->next != NULL) {
		pbuf_free(pwalk->next);

		pwalk->next = NULL;
	}

	/*
	 * Finally, hand off the packet to the layers above.  We go through
	 * ifdev so that it can pass the packet to BPF devices and update
	 * statistics and all that.
	 */
	ifdev_input(&ethif->ethif_ifdev, pbuf, NULL /*netif*/,
	    TRUE /*to_bpf*/);
}

/*
 * The ndev layer reports a network driver status update.  If anything has
 * changed since the last status, we may have to take action.  The given
 * statistics counters are relative to the previous status report.
 */
void
ethif_status(struct ethif * ethif, uint32_t link, uint32_t media,
	uint32_t oerror, uint32_t coll, uint32_t ierror, uint32_t iqdrop)
{
	struct if_data *ifdata;

	ethif_set_status(ethif, link, media);

	ifdata = ifdev_get_ifdata(&ethif->ethif_ifdev);
	ifdata->ifi_oerrors += oerror;
	ifdata->ifi_collisions += coll;
	ifdata->ifi_ierrors += ierror;
	ifdata->ifi_iqdrops += iqdrop;
}

/*
 * Set NetBSD-style interface flags (IFF_) for an ethernet interface.
 */
static int
ethif_set_ifflags(struct ifdev * ifdev, unsigned int ifflags)
{
	struct ethif *ethif = (struct ethif *)ifdev;
	uint32_t mode, flags;

	/*
	 * We do not support IFF_NOARP at this time, because lwIP does not: the
	 * idea of IFF_NOARP is that only static ARP entries are used, but lwIP
	 * does not support separating static from dynamic ARP operation.  The
	 * flag does not appear to be particularly widely used anyway.
	 */
	if ((ifflags & ~(IFF_UP | IFF_DEBUG | IFF_LINK0 | IFF_LINK1 |
	    IFF_LINK2)) != 0)
		return EINVAL;

	mode = ethif->ethif_wanted.nconf_mode;
	if ((ifflags & IFF_UP) && mode == NDEV_MODE_DOWN) {
		mode = NDEV_MODE_UP;

		/* Always enable broadcast receipt when supported. */
		if (ethif->ethif_caps & NDEV_CAP_BCAST)
			mode |= NDEV_MODE_BCAST;

		if (ifdev_is_promisc(ifdev))
			mode |= NDEV_MODE_PROMISC;

		/*
		 * The multicast flags will be set right before we send the
		 * request to the driver.
		 */
	} else if (!(ifflags & IFF_UP) && mode != NDEV_MODE_DOWN)
		ethif->ethif_wanted.nconf_mode = NDEV_MODE_DOWN;

	if (mode != ethif->ethif_wanted.nconf_mode) {
		ethif->ethif_wanted.nconf_mode = mode;
		ethif->ethif_wanted.nconf_set |= NDEV_SET_MODE;
	}

	/*
	 * Some of the interface flags (UP, DEBUG, PROMISC, LINK[0-2]) are a
	 * reflection of the intended state as set by userland before, so that
	 * a userland utility will never not see the flag it just set (or the
	 * other way around).  These flags therefore do not necessarily reflect
	 * what is actually going on at that moment.  We cannot have both.
	 */
	flags = 0;
	if (ifflags & IFF_DEBUG)
		flags |= NDEV_FLAG_DEBUG;
	if (ifflags & IFF_LINK0)
		flags |= NDEV_FLAG_LINK0;
	if (ifflags & IFF_LINK1)
		flags |= NDEV_FLAG_LINK1;
	if (ifflags & IFF_LINK2)
		flags |= NDEV_FLAG_LINK2;

	if (flags != ethif->ethif_wanted.nconf_flags) {
		ethif->ethif_wanted.nconf_flags = flags;
		ethif->ethif_wanted.nconf_set |= NDEV_SET_FLAGS;
	}

	/* The changes will be picked up from the polling function. */
	return OK;
}

/*
 * Convert a bitmask of ndev-layer capabilities (NDEV_CAP_) to NetBSD-style
 * interface capabilities (IFCAP_).
 */
static uint64_t
ethif_cap_to_ifcap(uint32_t caps)
{
	uint64_t ifcap;

	ifcap = 0;
	if (caps & NDEV_CAP_CS_IP4_TX)
		ifcap |= IFCAP_CSUM_IPv4_Tx;
	if (caps & NDEV_CAP_CS_IP4_RX)
		ifcap |= IFCAP_CSUM_IPv4_Rx;
	if (caps & NDEV_CAP_CS_UDP_TX)
		ifcap |= IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv6_Tx;
	if (caps & NDEV_CAP_CS_UDP_RX)
		ifcap |= IFCAP_CSUM_UDPv4_Rx | IFCAP_CSUM_UDPv6_Rx;
	if (caps & NDEV_CAP_CS_TCP_TX)
		ifcap |= IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv6_Tx;
	if (caps & NDEV_CAP_CS_TCP_RX)
		ifcap |= IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_TCPv6_Rx;

	return ifcap;
}

/*
 * Retrieve potential and enabled NetBSD-style interface capabilities (IFCAP_).
 */
static void
ethif_get_ifcap(struct ifdev * ifdev, uint64_t * ifcap, uint64_t * ifena)
{
	struct ethif *ethif = (struct ethif *)ifdev;

	*ifcap = ethif_cap_to_ifcap(ethif->ethif_caps);
	*ifena = ethif_cap_to_ifcap(ethif->ethif_wanted.nconf_caps);
}

/*
 * Set NetBSD-style enabled interface capabilities (IFCAP_).
 */
static int
ethif_set_ifcap(struct ifdev * ifdev, uint64_t ifcap)
{
	struct ethif *ethif = (struct ethif *)ifdev;
	unsigned int flags;
	uint32_t caps;

	if (ifcap & ~(IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
	    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv6_Tx |
	    IFCAP_CSUM_UDPv4_Rx | IFCAP_CSUM_UDPv6_Rx |
	    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv6_Tx |
	    IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_TCPv6_Rx))
		return EINVAL;

	/*
	 * Some IPv4/IPv6 flags need to be set together in order to be picked
	 * up.  Unfortunately, that is all we can do given that lwIP does not
	 * distinguish IPv4/IPv6 when it comes to TCP/UDP checksum flags.
	 */
	caps = 0;
	if (ifcap & IFCAP_CSUM_IPv4_Tx)
		caps |= NDEV_CAP_CS_IP4_TX;
	if (ifcap & IFCAP_CSUM_IPv4_Rx)
		caps |= NDEV_CAP_CS_IP4_RX;
	if ((ifcap & (IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv6_Tx)) ==
	    (IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv6_Tx))
		caps |= NDEV_CAP_CS_UDP_TX;
	if ((ifcap & (IFCAP_CSUM_UDPv4_Rx | IFCAP_CSUM_UDPv6_Rx)) ==
	    (IFCAP_CSUM_UDPv4_Rx | IFCAP_CSUM_UDPv6_Rx))
		caps |= NDEV_CAP_CS_UDP_RX;
	if ((ifcap & (IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv6_Tx)) ==
	    (IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv6_Tx))
		caps |= NDEV_CAP_CS_TCP_TX;
	if ((ifcap & (IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_TCPv6_Rx)) ==
	    (IFCAP_CSUM_TCPv4_Rx | IFCAP_CSUM_TCPv6_Rx))
		caps |= NDEV_CAP_CS_TCP_RX;

	/*
	 * When changing checksumming capabilities, we have to make sure that
	 * we only ever checksum too much and never too little.  This means
	 * that we enable any checksum options in netif here, and disable any
	 * checksum options in netif only after driver configuration.
	 *
	 * Note that we have to draw the line somewhere with this kind of
	 * self-protection, and that line is short of TCP retransmission: we
	 * see it as lwIP's job to compute checksums for retransmitted TCP
	 * packets if they were saved across checksum changes.  Even though
	 * lwIP may not care, there is little we can do about that anyway.
	 */
	if (ethif->ethif_wanted.nconf_caps != caps) {
		flags = ethif_get_netif(ethif)->chksum_flags;

		if (!(caps & NDEV_CAP_CS_IP4_TX))
			flags |= NETIF_CHECKSUM_GEN_IP;
		if (!(caps & NDEV_CAP_CS_IP4_RX))
			flags |= NETIF_CHECKSUM_CHECK_IP;
		if (!(caps & NDEV_CAP_CS_UDP_TX))
			flags |= NETIF_CHECKSUM_GEN_UDP;
		if (!(caps & NDEV_CAP_CS_UDP_RX))
			flags |= NETIF_CHECKSUM_CHECK_UDP;
		if (!(caps & NDEV_CAP_CS_TCP_TX))
			flags |= NETIF_CHECKSUM_GEN_TCP;
		if (!(caps & NDEV_CAP_CS_TCP_RX))
			flags |= NETIF_CHECKSUM_CHECK_TCP;

		NETIF_SET_CHECKSUM_CTRL(ethif_get_netif(ethif), flags);

		ethif->ethif_wanted.nconf_caps = caps;
		ethif->ethif_wanted.nconf_set |= NDEV_SET_CAPS;
	}

	/* The changes will be picked up from the polling function. */
	return OK;
}

/*
 * Retrieve NetBSD-style interface media type (IFM_).  Return both the current
 * media type selection and the driver-reported active media type.
 */
static void
ethif_get_ifmedia(struct ifdev * ifdev, int * ifcurrent, int * ifactive)
{
	struct ethif *ethif = (struct ethif *)ifdev;

	/*
	 * For the current select, report back whatever the user gave us, even
	 * if it has not reached the driver at all yet.
	 */
	*ifcurrent = (int)ethif->ethif_wanted.nconf_media;
	*ifactive = (int)ethif->ethif_media;
}

/*
 * Set current NetBSD-style interface media type (IFM_).
 */
static int
ethif_set_ifmedia(struct ifdev * ifdev, int ifmedia)
{
	struct ethif *ethif = (struct ethif *)ifdev;

	/*
	 * We currently completely lack the infrastructure to suspend the
	 * current IOCTL call until the driver replies (or disappears).
	 * Therefore we have no choice but to return success here, even if the
	 * driver cannot accept the change.  The driver does notify us of media
	 * changes, so the user may observe the new active media type later.
	 * Also note that the new media type may not be the requested type,
	 * which is why we do not perform any checks against the wanted or
	 * active media types.
	 */
	ethif->ethif_wanted.nconf_media = (uint32_t)ifmedia;
	ethif->ethif_wanted.nconf_set |= NDEV_SET_MEDIA;

	/* The change will be picked up from the polling function. */
	return OK;
}

/*
 * Enable or disable promiscuous mode on the interface.
 */
static void
ethif_set_promisc(struct ifdev * ifdev, int promisc)
{
	struct ethif *ethif = (struct ethif *)ifdev;

	if (ethif->ethif_wanted.nconf_mode != NDEV_MODE_DOWN) {
		if (promisc)
			ethif->ethif_wanted.nconf_mode |= NDEV_MODE_PROMISC;
		else
			ethif->ethif_wanted.nconf_mode &= ~NDEV_MODE_PROMISC;
		ethif->ethif_wanted.nconf_set |= NDEV_SET_MODE;
	}

	/* The change will be picked up from the polling function. */
}

/*
 * Set the hardware address on the interface.
 */
static int
ethif_set_hwaddr(struct ifdev * ifdev, const uint8_t * hwaddr)
{
	struct ethif *ethif = (struct ethif *)ifdev;

	if (!(ethif->ethif_caps & NDEV_CAP_HWADDR))
		return EINVAL;

	memcpy(&ethif->ethif_wanted.nconf_hwaddr.nhwa_addr, hwaddr,
	    ETHARP_HWADDR_LEN);
	ethif->ethif_wanted.nconf_set |= NDEV_SET_HWADDR;

	/* The change will be picked up from the polling function. */
	return OK;
}

/*
 * Set the Maximum Transmission Unit for this interface.  Return TRUE if the
 * new value is acceptable, in which case the caller will do the rest.  Return
 * FALSE otherwise.
 */
static int
ethif_set_mtu(struct ifdev * ifdev __unused, unsigned int mtu)
{

	return (mtu <= ETHIF_MAX_MTU);
}

static const struct ifdev_ops ethif_ops = {
	.iop_init = ethif_init_netif,
	.iop_input = netif_input,
	.iop_output = ethif_output,
	.iop_output_v4 = etharp_output,
	.iop_output_v6 = ethip6_output,
	.iop_hdrcmplt = ethif_hdrcmplt,
	.iop_poll = ethif_poll,
	.iop_set_ifflags = ethif_set_ifflags,
	.iop_get_ifcap = ethif_get_ifcap,
	.iop_set_ifcap = ethif_set_ifcap,
	.iop_get_ifmedia = ethif_get_ifmedia,
	.iop_set_ifmedia = ethif_set_ifmedia,
	.iop_set_promisc = ethif_set_promisc,
	.iop_set_hwaddr = ethif_set_hwaddr,
	.iop_set_mtu = ethif_set_mtu,
};
