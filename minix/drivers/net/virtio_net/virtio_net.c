/* virtio net driver for MINIX 3
 *
 * Copyright (c) 2013, A. Welzel, <arne.welzel@gmail.com>
 *
 * This software is released under the BSD license. See the LICENSE file
 * included in the main directory of this source distribution for the
 * license terms and conditions.
 */

#include <assert.h>
#include <sys/types.h>

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <minix/virtio.h>

#include <sys/queue.h>

#include "virtio_net.h"

#define VERBOSE 0

#if VERBOSE
#define dput(s)		do { dprintf(s); printf("\n"); } while (0)
#define dprintf(s) do {						\
	printf("%s: ", netdriver_name());			\
	printf s;						\
} while (0)
#else
#define dput(s)
#define dprintf(s)
#endif

static struct virtio_device *net_dev;

enum queue {RX_Q, TX_Q, CTRL_Q};

/* Number of packets to work with */
/* TODO: This should be an argument to the driver and possibly also
 *       depend on the queue sizes offered by this device.
 */
#define BUF_PACKETS		64
/* Maximum size of a packet */
#define MAX_PACK_SIZE		NDEV_ETH_PACKET_MAX
/* Buffer size needed for the payload of BUF_PACKETS */
#define PACKET_BUF_SZ		(BUF_PACKETS * MAX_PACK_SIZE)

struct packet {
	int idx;
	struct virtio_net_hdr *vhdr;
	phys_bytes phdr;
	char *vdata;
	phys_bytes pdata;
	size_t len;
	STAILQ_ENTRY(packet) next;
};

/* Allocated data chunks */
static char *data_vir;
static phys_bytes data_phys;
static struct virtio_net_hdr *hdrs_vir;
static phys_bytes hdrs_phys;
static struct packet *packets;
static int in_rx;

/* Packets on this list can be given to the host */
static STAILQ_HEAD(free_list, packet) free_list;

/* Packets on this list are to be given to inet */
static STAILQ_HEAD(recv_list, packet) recv_list;

/* Various state data */
static int spurious_interrupt;

/* Prototypes */
static int virtio_net_probe(unsigned int skip);
static void virtio_net_config(netdriver_addr_t *addr);
static int virtio_net_alloc_bufs(void);
static void virtio_net_init_queues(void);

static void virtio_net_refill_rx_queue(void);
static void virtio_net_check_queues(void);
static void virtio_net_check_pending(void);

static int virtio_net_init(unsigned int instance, netdriver_addr_t * addr,
	uint32_t * caps, unsigned int * ticks);
static void virtio_net_stop(void);
static int virtio_net_send(struct netdriver_data *data, size_t len);
static ssize_t virtio_net_recv(struct netdriver_data *data, size_t max);
static void virtio_net_intr(unsigned int mask);

static const struct netdriver virtio_net_table = {
	.ndr_name	= "vio",
	.ndr_init	= virtio_net_init,
	.ndr_stop	= virtio_net_stop,
	.ndr_recv	= virtio_net_recv,
	.ndr_send	= virtio_net_send,
	.ndr_intr	= virtio_net_intr,
};

/* TODO: Features are pretty much ignored */
static struct virtio_feature netf[] = {
	{ "partial csum",	VIRTIO_NET_F_CSUM,	0,	0	},
	{ "given mac",		VIRTIO_NET_F_MAC,	0,	1	},
	{ "status ",		VIRTIO_NET_F_STATUS,	0,	0	},
	{ "control channel",	VIRTIO_NET_F_CTRL_VQ,	0,	1	},
	{ "control channel rx",	VIRTIO_NET_F_CTRL_RX,	0,	0	}
};

static int
virtio_net_probe(unsigned int skip)
{
	/* virtio-net has at least 2 queues */
	int queues = 2;
	net_dev= virtio_setup_device(0x00001, netdriver_name(), netf,
				     sizeof(netf) / sizeof(netf[0]),
				     1 /* threads */, skip);
	if (net_dev == NULL)
		return ENXIO;

	/* If the host supports the control queue, allocate it as well */
	if (virtio_host_supports(net_dev, VIRTIO_NET_F_CTRL_VQ))
		queues += 1;

	if (virtio_alloc_queues(net_dev, queues) != OK) {
		virtio_free_device(net_dev);
		return ENOMEM;
	}

	return OK;
}

static void
virtio_net_config(netdriver_addr_t * addr)
{
	u32_t mac14;
	u32_t mac56;
	int i;

	if (virtio_host_supports(net_dev, VIRTIO_NET_F_MAC)) {
		dprintf(("Mac set by host: "));
		mac14 = virtio_sread32(net_dev, 0);
		mac56 = virtio_sread32(net_dev, 4);
		memcpy(&addr->na_addr[0], &mac14, 4);
		memcpy(&addr->na_addr[4], &mac56, 2);

		for (i = 0; i < 6; i++)
			dprintf(("%02x%s", addr->na_addr[i],
					 i == 5 ? "\n" : ":"));
	} else {
		dput(("No mac"));
	}

	if (virtio_host_supports(net_dev, VIRTIO_NET_F_STATUS)) {
		dput(("Current Status %x", (u32_t)virtio_sread16(net_dev, 6)));
	} else {
		dput(("No status"));
	}

	if (virtio_host_supports(net_dev, VIRTIO_NET_F_CTRL_VQ))
		dput(("Host supports control channel"));

	if (virtio_host_supports(net_dev, VIRTIO_NET_F_CTRL_RX))
		dput(("Host supports control channel for RX"));
}

static int
virtio_net_alloc_bufs(void)
{
	data_vir = alloc_contig(PACKET_BUF_SZ, 0, &data_phys);

	if (!data_vir)
		return ENOMEM;

	hdrs_vir = alloc_contig(BUF_PACKETS * sizeof(hdrs_vir[0]),
				 0, &hdrs_phys);

	if (!hdrs_vir) {
		free_contig(data_vir, PACKET_BUF_SZ);
		return ENOMEM;
	}

	packets = malloc(BUF_PACKETS * sizeof(packets[0]));

	if (!packets) {
		free_contig(data_vir, PACKET_BUF_SZ);
		free_contig(hdrs_vir, BUF_PACKETS * sizeof(hdrs_vir[0]));
		return ENOMEM;
	}

	memset(data_vir, 0, PACKET_BUF_SZ);
	memset(hdrs_vir, 0, BUF_PACKETS * sizeof(hdrs_vir[0]));
	memset(packets, 0, BUF_PACKETS * sizeof(packets[0]));

	return OK;
}

static void
virtio_net_init_queues(void)
{
	int i;
	STAILQ_INIT(&free_list);
	STAILQ_INIT(&recv_list);

	for (i = 0; i < BUF_PACKETS; i++) {
		packets[i].idx = i;
		packets[i].vhdr = &hdrs_vir[i];
		packets[i].phdr = hdrs_phys + i * sizeof(hdrs_vir[i]);
		packets[i].vdata = data_vir + i * MAX_PACK_SIZE;
		packets[i].pdata = data_phys + i * MAX_PACK_SIZE;
		STAILQ_INSERT_HEAD(&free_list, &packets[i], next);
	}
}

static void
virtio_net_refill_rx_queue(void)
{
	struct vumap_phys phys[2];
	struct packet *p;

	while ((in_rx < BUF_PACKETS / 2) && !STAILQ_EMPTY(&free_list)) {
		/* peek */
		p = STAILQ_FIRST(&free_list);
		/* remove */
		STAILQ_REMOVE_HEAD(&free_list, next);

		phys[0].vp_addr = p->phdr;
		assert(!(phys[0].vp_addr & 1));
		phys[0].vp_size = sizeof(struct virtio_net_hdr);

		phys[1].vp_addr = p->pdata;
		assert(!(phys[1].vp_addr & 1));
		phys[1].vp_size = MAX_PACK_SIZE;

		/* RX queue needs write */
		phys[0].vp_addr |= 1;
		phys[1].vp_addr |= 1;

		virtio_to_queue(net_dev, RX_Q, phys, 2, p);
		in_rx++;
	}

	if (in_rx == 0 && STAILQ_EMPTY(&free_list))
		dput(("warning: rx queue underflow!"));
}

static void
virtio_net_check_queues(void)
{
	struct packet *p;
	size_t len;

	/* Put the received packets into the recv list */
	while (virtio_from_queue(net_dev, RX_Q, (void **)&p, &len) == 0) {
		p->len = len;
		STAILQ_INSERT_TAIL(&recv_list, p, next);
		in_rx--;
	}

	/*
	 * Packets from the TX queue just indicated they are free to
	 * be reused now. inet already knows about them as being sent.
	 */
	while (virtio_from_queue(net_dev, TX_Q, (void **)&p, NULL) == 0) {
		memset(p->vhdr, 0, sizeof(*p->vhdr));
		memset(p->vdata, 0, MAX_PACK_SIZE);
		STAILQ_INSERT_HEAD(&free_list, p, next);
	}
}

static void
virtio_net_check_pending(void)
{

	/* Pending read and something in recv_list? */
	if (!STAILQ_EMPTY(&recv_list))
		netdriver_recv();

	if (!STAILQ_EMPTY(&free_list))
		netdriver_send();
}

static void
virtio_net_intr(unsigned int __unused mask)
{

	/* Check and clear interrupt flag */
	if (virtio_had_irq(net_dev)) {
		virtio_net_check_queues();
	} else {
		if (!spurious_interrupt)
			dput(("Spurious interrupt"));

		spurious_interrupt = 1;
	}

	virtio_net_check_pending();

	virtio_irq_enable(net_dev);

	/* Readd packets to the receive queue as necessary. */
	virtio_net_refill_rx_queue();
}

/*
 * Put user bytes into a free packet buffer, forward this packet to the TX
 * queue, and return OK.  If there are no free packet buffers, return SUSPEND.
 */
static int
virtio_net_send(struct netdriver_data * data, size_t len)
{
	struct vumap_phys phys[2];
	struct packet *p;

	if (STAILQ_EMPTY(&free_list))
		return SUSPEND;

	p = STAILQ_FIRST(&free_list);
	STAILQ_REMOVE_HEAD(&free_list, next);

	if (len > MAX_PACK_SIZE)
		panic("%s: packet too large to send: %zu",
		    netdriver_name(), len);

	netdriver_copyin(data, 0, p->vdata, len);

	phys[0].vp_addr = p->phdr;
	assert(!(phys[0].vp_addr & 1));
	phys[0].vp_size = sizeof(struct virtio_net_hdr);
	phys[1].vp_addr = p->pdata;
	assert(!(phys[1].vp_addr & 1));
	phys[1].vp_size = len;
	virtio_to_queue(net_dev, TX_Q, phys, 2, p);

	return OK;
}

/*
 * Put a packet receive from the RX queue into a user buffer, and return the
 * packet length.  If there are no received packets, return SUSPEND.
 */
static ssize_t
virtio_net_recv(struct netdriver_data * data, size_t max)
{
	struct packet *p;
	ssize_t len;

	/* Get the first received packet, if any. */
	if (STAILQ_EMPTY(&recv_list))
		return SUSPEND;

	p = STAILQ_FIRST(&recv_list);
	STAILQ_REMOVE_HEAD(&recv_list, next);

	/* Copy out the packet contents. */
	if (p->len < sizeof(struct virtio_net_hdr))
		panic("received packet does not have virtio header");
	len = p->len - sizeof(struct virtio_net_hdr);
	if ((size_t)len > max)
		len = (ssize_t)max;

	/*
	 * HACK: due to lack of padding, received packets may in fact be
	 * smaller than the minimum ethernet packet size.  The TCP/IP service
	 * will accept the packets just fine if we increase the length to its
	 * minimum.  We already zeroed out the rest of the packet data, so this
	 * is safe.
	 */
	if (len < NDEV_ETH_PACKET_MIN)
		len = NDEV_ETH_PACKET_MIN;

	netdriver_copyout(data, 0, p->vdata, len);

	/* Clean the packet. */
	memset(p->vhdr, 0, sizeof(*p->vhdr));
	memset(p->vdata, 0, MAX_PACK_SIZE);
	STAILQ_INSERT_HEAD(&free_list, p, next);

	/* Readd packets to the receive queue as necessary. */
	virtio_net_refill_rx_queue();

	return len;
}

/*
 * Initialize the driver and the virtual hardware.
 */
static int
virtio_net_init(unsigned int instance, netdriver_addr_t * addr,
	uint32_t * caps, unsigned int * ticks __unused)
{
	int r;

	if ((r = virtio_net_probe(instance)) != OK)
		return r;

	virtio_net_config(addr);

	if (virtio_net_alloc_bufs() != OK)
		panic("%s: Buffer allocation failed", netdriver_name());

	virtio_net_init_queues();

	/* Add packets to the receive queue. */
	virtio_net_refill_rx_queue();

	virtio_device_ready(net_dev);

	virtio_irq_enable(net_dev);

	*caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST;
	return OK;
}

/*
 * The driver is terminating.  Clean up.
 */
static void
virtio_net_stop(void)
{

	dput(("Terminating"));

	free_contig(data_vir, PACKET_BUF_SZ);
	free_contig(hdrs_vir, BUF_PACKETS * sizeof(hdrs_vir[0]));
	free(packets);

	virtio_reset_device(net_dev);
	virtio_free_queues(net_dev);
	virtio_free_device(net_dev);
	net_dev = NULL;
}

/*
 * The virtio-net device driver.
 */
int
main(int argc, char *argv[])
{

	env_setargs(argc, argv);

	netdriver_task(&virtio_net_table);

	return 0;
}
