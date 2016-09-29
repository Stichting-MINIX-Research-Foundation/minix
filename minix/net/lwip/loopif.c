/* LWIP service - loopif.c - loopback interfaces */
/*
 * There is always at least one loopback device.  This device is used also to
 * loop back packets sent on other interfaces to the local interface address.
 * Therefore, not all packets on the loopback device have a source or
 * destination address corresponding to the loopback device.
 */

#include "lwip.h"

/*
 * As a safety measure, if lwIP somehow gets stuck in a loop replying to its
 * own packets on a loopback interface, stop with immediately feeding packets
 * back into lwIP after this many packets.  The remaining packets will still be
 * delivered, but not before the main message loop has had a chance to run.
 */
#define LOOPIF_LIMIT	65536

/*
 * The MTU is restricted to 65531 bytes, because we need space for a 4-byte
 * header to identify the original interface of the packet.
 */
#define LOOPIF_MAX_MTU	(UINT16_MAX - sizeof(uint32_t))	/* maximum MTU */
#define LOOPIF_DEF_MTU	LOOPIF_MAX_MTU			/* default MTU */

#define NR_LOOPIF	2		/* number of loopback devices */

struct loopif {
	struct ifdev loopif_ifdev;	/* interface device, MUST be first */
	struct pbuf *loopif_head;	/* head of pending loopback packets */
	struct pbuf **loopif_tailp;	/* tail ptr-ptr of pending packets */
	TAILQ_ENTRY(loopif) loopif_next;	/* next in free list */
} loopif_array[NR_LOOPIF];

static TAILQ_HEAD(, loopif) loopif_freelist;	/* free loop interfaces list */
static TAILQ_HEAD(, loopif) loopif_activelist;	/* active loop interfaces */

#define loopif_get_netif(loopif) (ifdev_get_netif(&(loopif)->loopif_ifdev))

static unsigned int loopif_cksum_flags;

static int loopif_create(const char *name);

static const struct ifdev_ops loopif_ops;

/*
 * Initialize the loopback interface module.
 */
void
loopif_init(void)
{
	unsigned int slot;

	/* Initialize the lists of loopback interfaces. */
	TAILQ_INIT(&loopif_freelist);
	TAILQ_INIT(&loopif_activelist);

	for (slot = 0; slot < __arraycount(loopif_array); slot++)
		TAILQ_INSERT_TAIL(&loopif_freelist, &loopif_array[slot],
		    loopif_next);

	/*
	 * The default is to perform no checksumming on loopback interfaces,
	 * except for ICMP messages because otherwise we would need additional
	 * changes in the code receiving those.  In fact, for future
	 * compatibility, disable only those flags that we manage ourselves.
	 */
	loopif_cksum_flags = NETIF_CHECKSUM_ENABLE_ALL &
	    ~(NETIF_CHECKSUM_GEN_IP | NETIF_CHECKSUM_CHECK_IP |
	    NETIF_CHECKSUM_GEN_UDP | NETIF_CHECKSUM_CHECK_UDP |
	    NETIF_CHECKSUM_GEN_TCP | NETIF_CHECKSUM_CHECK_TCP);

	/* Tell the ifdev module that users may create more loopif devices. */
	ifdev_register("lo", loopif_create);
}

/*
 * Polling function, invoked after each message loop iteration.  Forward any
 * packets received on the output side of the loopback device during this
 * loop iteration, to the input side of the device.
 */
static void
loopif_poll(struct ifdev * ifdev)
{
	struct loopif *loopif = (struct loopif *)ifdev;
	struct pbuf *pbuf, **pnext;
	struct ifdev *oifdev;
	struct netif *netif;
	uint32_t oifindex;
	unsigned int count;
	static int warned = FALSE;

	count = 0;

	while ((pbuf = loopif->loopif_head) != NULL) {
		/*
		 * Prevent endless loops.  Keep in mind that packets may be
		 * added to the queue as part of processing packets from the
		 * queue here, so the queue itself will never reach this
		 * length.  As such the limit can (and must) be fairly high.
		 *
		 * In any case, if this warning is shown, that basically means
		 * that a bug in lwIP has been triggered.  There should be no
		 * such bugs, so if there are, they should be fixed in lwIP.
		 */
		if (count++ == LOOPIF_LIMIT) {
			if (!warned) {
				printf("LWIP: excess loopback traffic, "
				    "throttling output\n");
				warned = TRUE;
			}

			break;
		}

		pnext = pchain_end(pbuf);

		if ((loopif->loopif_head = *pnext) == NULL)
			loopif->loopif_tailp = &loopif->loopif_head;
		*pnext = NULL;

		/*
		 * Get the original interface for the packet, which if non-zero
		 * must also be used to pass the packet back to.  The interface
		 * should still exist in all cases, but better safe than sorry.
		 */
		memcpy(&oifindex, pbuf->payload, sizeof(oifindex));

		util_pbuf_header(pbuf, -(int)sizeof(oifindex));

		if (oifindex != 0 &&
		    (oifdev = ifdev_get_by_index(oifindex)) != NULL)
			netif = ifdev_get_netif(oifdev);
		else
			netif = NULL;

		/*
		 * Loopback devices hand packets to BPF on output only.  Doing
		 * so on input as well would duplicate all captured packets.
		 */
		ifdev_input(ifdev, pbuf, netif, FALSE /*to_bpf*/);
	}
}

/*
 * Process a packet as output on a loopback interface.  Packets cannot be
 * passed back into lwIP right away, nor can the original packets be passed
 * back into lwIP.  Therefore, make a copy of the packet, and pass it back to
 * lwIP at the end of the current message loop iteration.
 */
static err_t
loopif_output(struct ifdev * ifdev, struct pbuf * pbuf, struct netif * netif)
{
	struct loopif *loopif = (struct loopif *)ifdev;
	struct ifdev *oifdev;
	struct pbuf *pcopy;
	uint32_t oifindex;

	/* Reject oversized packets immediately.  This should not happen. */
	if (pbuf->tot_len > UINT16_MAX - sizeof(oifindex)) {
		printf("LWIP: attempt to send oversized loopback packet\n");

		return ERR_MEM;
	}

	/*
	 * If the service is low on memory, this is a likely place where
	 * allocation failures will occur.  Thus, do not print anything here.
	 * The user can diagnose such problems with interface statistics.
	 */
	pcopy = pchain_alloc(PBUF_RAW, sizeof(oifindex) + pbuf->tot_len);
	if (pcopy == NULL) {
		ifdev_output_drop(ifdev);

		return ERR_MEM;
	}

	/*
	 * If the packet was purposely diverted from a non-loopback interface
	 * to this interface, we have to remember the original interface, so
	 * that we can pass back the packet to that interface as well.  If we
	 * don't, packets to link-local addresses assigned to non-loopback
	 * interfaces will not be processed correctly.
	 */
	if (netif != NULL) {
		oifdev = netif_get_ifdev(netif);
		oifindex = ifdev_get_index(oifdev);
	} else
		oifindex = 0;

	assert(pcopy->len >= sizeof(oifindex));

	memcpy(pcopy->payload, &oifindex, sizeof(oifindex));

	util_pbuf_header(pcopy, -(int)sizeof(oifindex));

	if (pbuf_copy(pcopy, pbuf) != ERR_OK)
		panic("unexpected pbuf copy failure");

	pcopy->flags |= pbuf->flags & (PBUF_FLAG_LLMCAST | PBUF_FLAG_LLBCAST);

	util_pbuf_header(pcopy, sizeof(oifindex));

	*loopif->loopif_tailp = pcopy;
	loopif->loopif_tailp = pchain_end(pcopy);

	return ERR_OK;
}

/*
 * Initialization function for a loopback-type netif interface, called from
 * lwIP at interface creation time.
 */
static err_t
loopif_init_netif(struct ifdev * ifdev, struct netif * netif)
{

	netif->name[0] = 'l';
	netif->name[1] = 'o';

	/*
	 * FIXME: unfortunately, lwIP does not allow one to enable multicast on
	 * an interface without also enabling multicast management traffic
	 * (that is, IGMP and MLD).  Thus, for now, joining multicast groups
	 * and assigning local IPv6 addresses will incur such traffic even on
	 * loopback interfaces.  For now this is preferable over not supporting
	 * multicast on loopback interfaces at all.
	 */
	netif->flags |= NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;

	NETIF_SET_CHECKSUM_CTRL(netif, loopif_cksum_flags);

	return ERR_OK;
}

/*
 * Create a new loopback device.
 */
static int
loopif_create(const char * name)
{
	struct loopif *loopif;

	/* Find a free loopback interface slot, if available. */
	if (TAILQ_EMPTY(&loopif_freelist))
		return ENOBUFS;

	loopif = TAILQ_FIRST(&loopif_freelist);
	TAILQ_REMOVE(&loopif_freelist, loopif, loopif_next);

	/* Initialize the loopif structure. */
	TAILQ_INSERT_HEAD(&loopif_activelist, loopif, loopif_next);

	loopif->loopif_head = NULL;
	loopif->loopif_tailp = &loopif->loopif_head;

	/*
	 * For simplicity and efficiency, we do not prepend the address family
	 * (IPv4/IPv6) to the packet for BPF, which means our loopback devices
	 * are of type DLT_RAW rather than (NetBSD's) DLT_NULL.
	 */
	ifdev_add(&loopif->loopif_ifdev, name, IFF_LOOPBACK | IFF_MULTICAST,
	    IFT_LOOP, 0 /*hdrlen*/, 0 /*addrlen*/, DLT_RAW, LOOPIF_MAX_MTU,
	    0 /*nd6flags*/, &loopif_ops);

	ifdev_update_link(&loopif->loopif_ifdev, LINK_STATE_UP);

	return OK;
}

/*
 * Destroy an existing loopback device.
 */
static int
loopif_destroy(struct ifdev * ifdev)
{
	struct loopif *loopif = (struct loopif *)ifdev;
	struct pbuf *pbuf, **pnext;
	int r;

	/*
	 * The ifdev module may refuse to remove this interface if it is the
	 * loopback interface used to loop back packets for other interfaces.
	 */
	if ((r = ifdev_remove(&loopif->loopif_ifdev)) != OK)
		return r;

	/*
	 * Clean up.  The loopback queue can be non-empty only if we have been
	 * throttling in case of a feedback loop.
	 */
	while ((pbuf = loopif->loopif_head) != NULL) {
		pnext = pchain_end(pbuf);

		if ((loopif->loopif_head = *pnext) == NULL)
			loopif->loopif_tailp = &loopif->loopif_head;
		*pnext = NULL;

		pbuf_free(pbuf);
	}

	TAILQ_REMOVE(&loopif_activelist, loopif, loopif_next);

	TAILQ_INSERT_HEAD(&loopif_freelist, loopif, loopif_next);

	return OK;
}

/*
 * Set NetBSD-style interface flags (IFF_) for a loopback interface.
 */
static int
loopif_set_ifflags(struct ifdev * ifdev, unsigned int ifflags)
{
	struct loopif *loopif = (struct loopif *)ifdev;

	/*
	 * Only the IFF_UP flag may be set and cleared.  We adjust the
	 * IFF_RUNNING flag immediately based on this flag.  This is a bit
	 * dangerous, but the caller takes this possibility into account.
	 */
	if ((ifflags & ~IFF_UP) != 0)
		return EINVAL;

	if (ifflags & IFF_UP)
		ifdev_update_ifflags(&loopif->loopif_ifdev,
		    ifdev_get_ifflags(&loopif->loopif_ifdev) | IFF_RUNNING);
	else
		ifdev_update_ifflags(&loopif->loopif_ifdev,
		    ifdev_get_ifflags(&loopif->loopif_ifdev) & ~IFF_RUNNING);

	return OK;
}

/*
 * Set the Maximum Transmission Unit for this interface.  Return TRUE if the
 * new value is acceptable, in which case the caller will do the rest.  Return
 * FALSE otherwise.
 */
static int
loopif_set_mtu(struct ifdev * ifdev __unused, unsigned int mtu)
{

	return (mtu <= LOOPIF_MAX_MTU);
}

static const struct ifdev_ops loopif_ops = {
	.iop_init = loopif_init_netif,
	.iop_input = ip_input,
	.iop_output = loopif_output,
	.iop_poll = loopif_poll,
	.iop_set_ifflags = loopif_set_ifflags,
	.iop_set_mtu = loopif_set_mtu,
	.iop_destroy = loopif_destroy,
};

/*
 * Set and/or retrieve a per-protocol loopback checksumming option through
 * sysctl(7).
 */
ssize_t
loopif_cksum(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp)
{
	struct loopif *loopif;
	unsigned int flags;
	int r, val;

	/*
	 * The third name field is the protocol.  We ignore the domain (the
	 * second field), thus sharing settings between PF_INET and PF_INET6.
	 * This is necessary because lwIP does not support TCP/UDP checksumming
	 * flags on a per-domain basis.
	 */
	switch (call->call_oname[2]) {
	case IPPROTO_IP:
		flags = NETIF_CHECKSUM_GEN_IP | NETIF_CHECKSUM_CHECK_IP;
		break;
	case IPPROTO_UDP:
		flags = NETIF_CHECKSUM_GEN_UDP | NETIF_CHECKSUM_CHECK_UDP;
		break;
	case IPPROTO_TCP:
		flags = NETIF_CHECKSUM_GEN_TCP | NETIF_CHECKSUM_CHECK_TCP;
		break;
	default:
		return EINVAL;
	}

	/* Copy out the old (current) checksumming option. */
	if (oldp != NULL) {
		val = !!(loopif_cksum_flags & flags);

		if ((r = rmib_copyout(oldp, 0, &val, sizeof(val))) < 0)
			return r;
	}

	if (newp != NULL) {
		if ((r = rmib_copyin(newp, &val, sizeof(val))) != OK)
			return r;

		if (val)
			loopif_cksum_flags |= flags;
		else
			loopif_cksum_flags &= ~flags;

		/*
		 * Apply the new checksum flags to all loopback interfaces.
		 * Technically, this may result in dropped packets when
		 * enabling checksumming on a throttled loopif, but that is a
		 * case so rare and unimportant that we ignore it.
		 */
		TAILQ_FOREACH(loopif, &loopif_activelist, loopif_next) {
			NETIF_SET_CHECKSUM_CTRL(loopif_get_netif(loopif),
			    loopif_cksum_flags);
		}
	}

	/* Return the length of the node. */
	return sizeof(val);
}
