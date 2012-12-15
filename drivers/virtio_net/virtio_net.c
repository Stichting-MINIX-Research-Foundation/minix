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

#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <minix/sysutil.h>
#include <minix/virtio.h>

#include <sys/queue.h>

#include "virtio_net.h"

#define dput(s)		do { dprintf(s); printf("\n"); } while (0)
#define dprintf(s) do {						\
	printf("%s: ", name);					\
	printf s;						\
} while (0)

static struct virtio_device *net_dev;

static const char *const name = "virtio-net";

enum queue {RX_Q, TX_Q, CTRL_Q};

/* Number of packets to work with */
/* TODO: This should be an argument to the driver and possibly also
 *       depend on the queue sizes offered by this device.
 */
#define BUF_PACKETS		64
/* Maximum size of a packet */
#define MAX_PACK_SIZE		ETH_MAX_PACK_SIZE
/* Buffer size needed for the payload of BUF_PACKETS */
#define PACKET_BUF_SZ		(BUF_PACKETS * MAX_PACK_SIZE)

struct packet {
	int idx;
	struct virtio_net_hdr *vhdr;
	phys_bytes phdr;
	char *vdata;
	phys_bytes pdata;
	STAILQ_ENTRY(packet) next;
};

/* Allocated data chunks */
static char *data_vir;
static phys_bytes data_phys;
static struct virtio_net_hdr *hdrs_vir;
static phys_bytes hdrs_phys;
static struct packet *packets;
static int in_rx;
static int started;

/* Packets on this list can be given to the host */
static STAILQ_HEAD(free_list, packet) free_list;

/* Packets on this list are to be given to inet */
static STAILQ_HEAD(recv_list, packet) recv_list;

/* State about pending inet messages */
static int rx_pending;
static message pending_rx_msg;
static int tx_pending;
static message pending_tx_msg;

/* Various state data */
static u8_t virtio_net_mac[6];
static eth_stat_t virtio_net_stats;
static int spurious_interrupt;


/* Prototypes */
static int virtio_net_probe(int skip);
static int virtio_net_config(void);
static int virtio_net_alloc_bufs(void);
static void virtio_net_init_queues(void);

static void virtio_net_refill_rx_queue(void);
static void virtio_net_check_queues(void);
static void virtio_net_check_pending(void);

static void virtio_net_fetch_iovec(iovec_s_t *iov, message *m);
static int virtio_net_cpy_to_user(message *m);
static int virtio_net_cpy_from_user(message *m);

static void virtio_net_intr(message *m);
static void virtio_net_write(message *m);
static void virtio_net_read(message *m);
static void virtio_net_conf(message *m);
static void virtio_net_getstat(message *m);

static void virtio_net_notify(message *m);
static void virtio_net_msg(message *m);
static void virtio_net_main_loop(void);

static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);


/* TODO: Features are pretty much ignored */
struct virtio_feature netf[] = {
	{ "partial csum",	VIRTIO_NET_F_CSUM,	0,	0	},
	{ "given mac",		VIRTIO_NET_F_MAC,	0,	0	},
	{ "status ",		VIRTIO_NET_F_STATUS,	0,	0	},
	{ "control channel",	VIRTIO_NET_F_CTRL_VQ,	0,	0	},
	{ "control channel rx",	VIRTIO_NET_F_CTRL_RX,	0,	0	}
};

static int
virtio_net_probe(int skip)
{
	/* virtio-net has at least 2 queues */
	int queues = 2;
	net_dev= virtio_setup_device(0x00001, name, netf,
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

static int
virtio_net_config(void)
{
	u32_t mac14;
	u32_t mac56;
	int i;

	if (virtio_host_supports(net_dev, VIRTIO_NET_F_MAC)) {
		dprintf(("Mac set by host: "));
		mac14 = virtio_sread32(net_dev, 0);
		mac56 = virtio_sread32(net_dev, 4);
		*(u32_t*)virtio_net_mac = mac14;
		*(u16_t*)(virtio_net_mac + 4) = mac56;

		for (i = 0; i < 6; i++)
			printf("%02x%s", virtio_net_mac[i],
					 i == 5 ? "\n" : ":");
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

	return OK;
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

	if (in_rx == 0 && STAILQ_EMPTY(&free_list)) {
		dput(("warning: rx queue underflow!"));
		virtio_net_stats.ets_fifoUnder++;
	}
}

static void
virtio_net_check_queues(void)
{
	struct packet *p;

	/* Put the received packets into the recv list */
	while (virtio_from_queue(net_dev, RX_Q, (void **)&p) == 0) {
		STAILQ_INSERT_TAIL(&recv_list, p, next);
		in_rx--;
		virtio_net_stats.ets_packetR++;
	}

	/* Packets from the TX queue just indicated they are free to
	 * be reused now. inet already knows about them as being sent.
	 */
	while (virtio_from_queue(net_dev, TX_Q, (void **)&p) == 0) {
		memset(p->vhdr, 0, sizeof(*p->vhdr));
		memset(p->vdata, 0, MAX_PACK_SIZE);
		STAILQ_INSERT_HEAD(&free_list, p, next);
		virtio_net_stats.ets_packetT++;
	}
}

static void
virtio_net_check_pending(void)
{
	int dst = 0xDEAD;
	int r;

	message reply;
	reply.m_type = DL_TASK_REPLY;
	reply.DL_FLAGS = DL_NOFLAGS;
	reply.DL_COUNT = 0;

	/* Pending read and something in recv_list? */
	if (!STAILQ_EMPTY(&recv_list) && rx_pending) {
		dst = pending_rx_msg.m_source;
		reply.DL_COUNT = virtio_net_cpy_to_user(&pending_rx_msg);
		reply.DL_FLAGS |= DL_PACK_RECV;
		rx_pending = 0;
	}

	if (!STAILQ_EMPTY(&free_list) && tx_pending) {
		dst = pending_tx_msg.m_source;
		virtio_net_cpy_from_user(&pending_tx_msg);
		reply.DL_FLAGS |= DL_PACK_SEND;
		tx_pending = 0;
	}

	/* Only reply if a pending request was handled */
	if (reply.DL_FLAGS != DL_NOFLAGS)
		if ((r = send(dst, &reply)) != OK)
			panic("%s: send to %d failed (%d)", name, dst, r);
}

static void
virtio_net_fetch_iovec(iovec_s_t *iov, message *m)
{
	int r;
	r = sys_safecopyfrom(m->m_source, m->DL_GRANT, 0, (vir_bytes)iov,
			     m->DL_COUNT * sizeof(iov[0]));

	if (r != OK)
		panic("%s: iovec fail for %d (%d)", name, m->m_source, r);
}

static int
virtio_net_cpy_to_user(message *m)
{
	/* Hmm, this looks so similar to cpy_from_user... TODO */
	int i, r, size, ivsz;
	int left = MAX_PACK_SIZE;	/* Try copying the whole packet */
	int bytes = 0;
	iovec_s_t iovec[NR_IOREQS];
	struct packet *p;

	/* This should only be called if recv_list has some entries */
	assert(!STAILQ_EMPTY(&recv_list));

	p = STAILQ_FIRST(&recv_list);
	STAILQ_REMOVE_HEAD(&recv_list, next);

	virtio_net_fetch_iovec(iovec, m);

	for (i = 0; i < m->DL_COUNT && left > 0; i++) {
		ivsz = iovec[i].iov_size;
		size = left > ivsz ? ivsz : left;
		r = sys_safecopyto(m->m_source, iovec[i].iov_grant, 0,
				   (vir_bytes) p->vdata + bytes, size);

		if (r != OK)
			panic("%s: copy to %d failed (%d)", name,
							    m->m_source,
							    r);

		left -= size;
		bytes += size;
	}

	if (left != 0)
		dput(("Uhm... left=%d", left));

	/* Clean the packet */
	memset(p->vhdr, 0, sizeof(*p->vhdr));
	memset(p->vdata, 0, MAX_PACK_SIZE);
	STAILQ_INSERT_HEAD(&free_list, p, next);

	return bytes;
}

static int
sys_easy_vsafecopy_from(endpoint_t src_proc, iovec_s_t *iov, int count,
			vir_bytes dst, size_t max, size_t *copied)
{
	int i, r;
	size_t left = max;
	vir_bytes cur_off = 0;
	struct vscp_vec vv[NR_IOREQS];

	for (i = 0; i < count && left > 0; i++) {
		vv[i].v_from = src_proc;
		vv[i].v_to = SELF;
		vv[i].v_gid = iov[i].iov_grant;
		vv[i].v_offset = 0;
		vv[i].v_addr = dst + cur_off;
		vv[i].v_bytes = iov[i].iov_size;

		/* More data in iov than the buffer can hold, this should be
		 * manageable by the caller.
		 */
		if (left - vv[i].v_bytes > left) {
			printf("sys_easy_vsafecopy_from: buf too small!\n");
			return ENOMEM;
		}

		left -= iov[i].iov_size;
		cur_off += iov[i].iov_size;
	}

	/* Now that we prepared the vscp_vec, we can call vsafecopy() */
	if ((r = sys_vsafecopy(vv, count)) != OK)
		printf("sys_vsafecopy: failed: (%d)\n", r);

	if (copied)
		*copied = cur_off;

	return OK;
}

static int
virtio_net_cpy_from_user(message *m)
{
	/* Put user bytes into a a free packet buffer and
	 * then forward this packet to the TX queue.
	 */
	int r;
	iovec_s_t iovec[NR_IOREQS];
	struct vumap_phys phys[2];
	struct packet *p;
	size_t bytes;

	/* This should only be called if free_list has some entries */
	assert(!STAILQ_EMPTY(&free_list));

	p = STAILQ_FIRST(&free_list);
	STAILQ_REMOVE_HEAD(&free_list, next);

	virtio_net_fetch_iovec(iovec, m);

	r = sys_easy_vsafecopy_from(m->m_source, iovec, m->DL_COUNT,
				    (vir_bytes)p->vdata, MAX_PACK_SIZE,
				    &bytes);

	if (r != OK)
		panic("%s: copy from %d failed", name, m->m_source);


	phys[0].vp_addr = p->phdr;
	assert(!(phys[0].vp_addr & 1));
	phys[0].vp_size = sizeof(struct virtio_net_hdr);
	phys[1].vp_addr = p->pdata;
	assert(!(phys[1].vp_addr & 1));
	phys[1].vp_size = bytes;
	virtio_to_queue(net_dev, TX_Q, phys, 2, p);
	return bytes;
}

static void
virtio_net_intr(message *m)
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
}

static void
virtio_net_write(message *m)
{
	int r;
	message reply;

	reply.m_type = DL_TASK_REPLY;
	reply.DL_FLAGS = DL_NOFLAGS;
	reply.DL_COUNT = 0;


	if (!STAILQ_EMPTY(&free_list)) {
		/* free_list contains at least one  packet, use it */
		reply.DL_COUNT = virtio_net_cpy_from_user(m);
		reply.DL_FLAGS = DL_PACK_SEND;
	} else {
		pending_tx_msg = *m;
		tx_pending = 1;
	}

	if ((r = send(m->m_source, &reply)) != OK)
		panic("%s: send to %d failed (%d)", name, m->m_source, r);
}

static void
virtio_net_read(message *m)
{
	int r;
	message reply;

	reply.m_type = DL_TASK_REPLY;
	reply.DL_FLAGS = DL_NOFLAGS;
	reply.DL_COUNT = 0;

	if (!STAILQ_EMPTY(&recv_list)) {
		/* recv_list contains at least one  packet, copy it */
		reply.DL_COUNT = virtio_net_cpy_to_user(m);
		reply.DL_FLAGS = DL_PACK_RECV;
	} else {
		rx_pending = 1;
		pending_rx_msg = *m;
	}

	if ((r = send(m->m_source, &reply)) != OK)
		panic("%s: send to %d failed (%d)", name, m->m_source, r);
}

static void
virtio_net_conf(message *m)
{
	/* TODO: Add the multicast, broadcast filtering etc. */
	int i, r;

	message reply;

	/* If this is the first CONF message we see, fully initialize
	 * the device now.
	 */
	if (!started) {
		started = 1;
		virtio_device_ready(net_dev);
		virtio_irq_enable(net_dev);
	}

	/* Prepare reply */
	for (i = 0; i < sizeof(virtio_net_mac); i++)
		((u8_t*)reply.DL_HWADDR)[i] = virtio_net_mac[i];

	reply.m_type = DL_CONF_REPLY;
	reply.DL_STAT = OK;
	reply.DL_COUNT = 0;

	if ((r = send(m->m_source, &reply)) != OK)
		panic("%s: send to %d failed (%d)", name, m->m_source, r);
}

static void
virtio_net_getstat(message *m)
{
	int r;
	message reply;

	reply.m_type = DL_STAT_REPLY;
	reply.DL_STAT = OK;
	reply.DL_COUNT = 0;


	r = sys_safecopyto(m->m_source, m->DL_GRANT, 0,
			   (vir_bytes)&virtio_net_stats,
			   sizeof(virtio_net_stats));

	if (r != OK)
		panic("%s: copy to %d failed (%d)", name, m->m_source, r);

	if ((r = send(m->m_source, &reply)) != OK)
		panic("%s: send to %d failed (%d)", name, m->m_source, r);
}

static void
virtio_net_notify(message *m)
{
	if (_ENDPOINT_P(m->m_source) == HARDWARE)
		virtio_net_intr(m);
}

static void
virtio_net_msg(message *m)
{
	switch (m->m_type) {
	case DL_WRITEV_S:
		virtio_net_write(m);
		break;
	case DL_READV_S:
		virtio_net_read(m);
		break;
	case DL_CONF:
		virtio_net_conf(m);
		break;
	case DL_GETSTAT_S:
		virtio_net_getstat(m);
		break;
	default:
		panic("%s: illegal message: %d", name, m->m_type);
	}
}

static void
virtio_net_main_loop(void)
{
	message m;
	int ipc_status;
	int r;

	while (TRUE) {

		virtio_net_refill_rx_queue();

		if ((r = netdriver_receive(ANY, &m, &ipc_status)) != OK)
			panic("%s: netdriver_receive failed: %d", name, r);

		if (is_ipc_notify(ipc_status))
			virtio_net_notify(&m);
		else
			virtio_net_msg(&m);
	}
}

int
main(int argc, char *argv[])
{
	env_setargs(argc, argv);
	sef_local_startup();

	virtio_net_main_loop();
}

static void
sef_local_startup()
{
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_lu(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
	sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_workfree);

	sef_setcb_signal_handler(sef_cb_signal_handler);

	sef_startup();
}

static int
sef_cb_init_fresh(int type, sef_init_info_t *info)
{
	long instance = 0;
	env_parse("instance", "d", 0, &instance, 0, 255);

	if (virtio_net_probe((int)instance) != OK)
		panic("%s: No device found", name);

	if (virtio_net_config() != OK)
		panic("%s: No device found", name);

	if (virtio_net_alloc_bufs() != OK)
		panic("%s: Buffer allocation failed", name);

	virtio_net_init_queues();

	netdriver_announce();

	return(OK);
}

static void
sef_cb_signal_handler(int signo)
{
	if (signo != SIGTERM)
		return;

	dput(("Terminating"));

	free_contig(data_vir, PACKET_BUF_SZ);
	free_contig(hdrs_vir, BUF_PACKETS * sizeof(hdrs_vir[0]));
	free(packets);

	virtio_reset_device(net_dev);
	virtio_free_queues(net_dev);
	virtio_free_device(net_dev);
	net_dev = NULL;

	exit(1);
}
