/*
 * This file implements handling of meesagges send by drivers
 */

#include <stdio.h>
#include <stdlib.h>

#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/sysutil.h>
#include <minix/safecopies.h>
#include <minix/netsock.h>

#include <sys/ioc_net.h>
#include <net/gen/in.h>
#include <net/gen/ip_io.h>
#include <net/gen/route.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <netif/etharp.h>

#include "proto.h"
#include "driver.h"

#if 0
#define debug_drv_print(str, ...) printf("LWIP %s:%d : " str "\n", \
		__func__, __LINE__, ##__VA_ARGS__)
#else
#define debug_drv_print(...) debug_print(__VA_ARGS__)
#endif

#define RAW_BUF_SIZE	(32 << 10)

static struct nic devices[MAX_DEVS];

static ip_addr_t ip_addr_none = { IPADDR_NONE };
extern endpoint_t lwip_ep;

void nic_assign_driver(const char * dev_type,
			unsigned int dev_num,
			const char * driver_name,
			unsigned int instance,
			int is_default)
{
	struct nic * nic;

	if (strcmp(dev_type, "eth") != 0) {
		printf("LWIP : Cannot handle other than ethernet devices, "
				"ignoring '%s%d'\n", dev_type, dev_num);
		return;
	}

	nic = &devices[dev_num];
	snprintf(nic->name, NIC_NAME_LEN, "%s%d", dev_type, dev_num);
	nic->name[NIC_NAME_LEN - 1] = '\0';
	snprintf(nic->drv_name, DRV_NAME_LEN, "%s_%d", driver_name, instance);
	nic->drv_name[DRV_NAME_LEN - 1] = '\0';
	nic->is_default = is_default;
	nic->netif.name[0] = 'e';
	nic->netif.name[1] = 't';
	nic->netif.num = dev_num;

	debug_print("/dev/%s driven by %s default = %d",
			nic->name, nic->drv_name, is_default);
}

static struct nic * lookup_nic_by_drv_ep(endpoint_t ep)
{
	int i;

	for (i = 0; i < MAX_DEVS; i++) {
		if (devices[i].drv_ep == ep)
			return &devices[i];
	}

	return NULL;
}

static struct nic * lookup_nic_by_drv_name(const char * name)
{
	int i;

	for (i = 0; i < MAX_DEVS; i++) {
		if (strcmp(devices[i].drv_name, name) == 0)
			return &devices[i];
	}

	return NULL;
}

static struct nic * lookup_nic_default(void)
{
	int i;

	for (i = 0; i < MAX_DEVS; i++) {
		if (devices[i].is_default)
			return &devices[i];
	}

	return NULL;
}

void nic_init_all(void)
{
	int i;
	unsigned int g;

	for (i = 0; i < MAX_DEVS; i++) {
		devices[i].drv_ep = NONE;
		devices[i].is_default = 0;

		if (cpf_getgrants(&devices[i].rx_iogrant, 1) != 1)
			panic("Cannot initialize grants");
		if (cpf_getgrants(&devices[i].rx_iovec[0].iov_grant, 1) != 1)
			panic("Cannot initialize grants");
		if (cpf_getgrants(&devices[i].tx_iogrant, 1) != 1)
			panic("Cannot initialize grants");
		for (g = 0; g < TX_IOVEC_NUM; g++) {
			cp_grant_id_t * gid = &devices[i].tx_iovec[g].iov_grant;
			if (cpf_getgrants(gid, 1) != 1)
				panic("Cannot initialize grants");
		}
		devices[i].raw_socket = NULL;
	}
}

static void driver_setup_read(struct nic * nic)
{
	message m;

	debug_print("device /dev/%s", nic->name);
	//assert(nic->rx_pbuf == NULL);
	if (!(nic->rx_pbuf == NULL)) {
		panic("device /dev/%s rx_pbuf %p", nic->name, nic->rx_pbuf);
	}

	if (!(nic->rx_pbuf = pbuf_alloc(PBUF_RAW, ETH_MAX_PACK_SIZE + ETH_CRC_SIZE, PBUF_RAM)))
		panic("Cannot allocate rx pbuf");

	if (cpf_setgrant_direct(nic->rx_iovec[0].iov_grant,
				nic->drv_ep, (vir_bytes) nic->rx_pbuf->payload,
				nic->rx_pbuf->len, CPF_WRITE) != OK)
		panic("Failed to set grant");
	nic->rx_iovec[0].iov_size = nic->rx_pbuf->len;

	m.m_type = DL_READV_S;
	m.m_net_netdrv_dl_readv_s.count = 1;
	m.m_net_netdrv_dl_readv_s.grant = nic->rx_iogrant;

	if (asynsend(nic->drv_ep, &m) != OK)
		panic("asynsend to the driver failed!");
}

static void nic_up(struct nic * nic, message * m)
{
	memcpy(nic->netif.hwaddr, m->m_netdrv_net_dl_conf.hw_addr,
		sizeof(nic->netif.hwaddr));

	debug_print("device %s is up MAC : %02x:%02x:%02x:%02x:%02x:%02x",
			nic->name,
			nic->netif.hwaddr[0],
			nic->netif.hwaddr[1],
			nic->netif.hwaddr[2],
			nic->netif.hwaddr[3],
			nic->netif.hwaddr[4],
			nic->netif.hwaddr[5]);

	driver_setup_read(nic);

	netif_set_link_up(&nic->netif);
	netif_set_up(&nic->netif);
}

int driver_tx(struct nic * nic)
{
	struct packet_q * pkt;
	unsigned int len;
	message m;

	int err;

	debug_print("device /dev/%s", nic->name);
	assert(nic->tx_buffer);

	pkt = driver_tx_head(nic);
	if (pkt == NULL) {
		debug_print("no packets enqueued");
		return 0;
	}

	assert(pkt->buf_len <= nic->max_pkt_sz);
	
	if ((len = pkt->buf_len) < nic->min_pkt_sz)
		len = nic->min_pkt_sz;
	err = cpf_setgrant_direct(nic->tx_iovec[0].iov_grant,
			nic->drv_ep, (vir_bytes) pkt->buf,
			len, CPF_READ);
	debug_print("packet len %d", len);
	if (err != OK)
		panic("Failed to set grant");
	nic->tx_iovec[0].iov_size = len;
	
	if (cpf_setgrant_direct(nic->tx_iogrant, nic->drv_ep,
			(vir_bytes) &nic->tx_iovec,
			sizeof(iovec_s_t), CPF_READ) != OK)
		panic("Failed to set grant");

	m.m_type = DL_WRITEV_S;
	m.m_net_netdrv_dl_writev_s.count = 1;
	m.m_net_netdrv_dl_writev_s.grant = nic->tx_iogrant;

	if (asynsend(nic->drv_ep, &m) != OK)
		panic("asynsend to the driver failed!");
	nic->state = DRV_SENDING;
	
	debug_print("packet sent to driver");

	return 1;
}

static void nic_pkt_sent(struct nic * nic)
{
	debug_print("device /dev/%s", nic->name);
	assert(nic->state != DRV_IDLE);

	/* packet has been sent, we are not intereted anymore */
	driver_tx_dequeue(nic);
	/*
	 * Try to transmit the next packet. Failure means that no packet is
	 * enqueued and thus the device is entering idle state
	 */
	if (!driver_tx(nic))
		nic->state = DRV_IDLE;
}

__unused static void print_pkt(unsigned char * pkt, int len)
{
	int i = 0;

	printf("--- PKT ---\n");

	while (i < len) {
		int x;

		for (x = 0; x < 8 && i < len; x++, i++)
			printf("%02x ", pkt[i]);

		kputc(' ');

		for (x = 0; x < 8 && i < len; x++, i++)
			printf("%02x ", pkt[i]);

		kputc('\n');
	}

	printf("--- PKT END ---\n");
}

static int raw_receive(struct sock_req *req,
			struct pbuf *pbuf)
{
	struct pbuf * p;
	unsigned int rem_len = req->size;
	unsigned int written = 0;
	int err;

	debug_print("user buffer size : %d\n", rem_len);

	for (p = pbuf; p && rem_len; p = p->next) {
		size_t cp_len;

		cp_len = (rem_len < p->len) ? rem_len : p->len;
		err = copy_to_user(req->endpt, p->payload, cp_len,
				   req->grant, written);
		if (err != OK)
			return err;

		written += cp_len;
		rem_len -= cp_len;
	}

	debug_print("copied %d bytes\n", written);
	return written;
}

int raw_socket_input(struct pbuf * pbuf, struct nic * nic)
{
	struct socket * sock;
	struct pbuf * pbuf_new;

	if ((sock = nic->raw_socket) == NULL)
		return 0;

	debug_print("socket num : %ld", get_sock_num(sock));

	if (sock->flags & SOCK_FLG_OP_PENDING) {
		int ret;
		/* we are resuming a suspended operation */
		ret = raw_receive(&sock->req, pbuf);

		send_req_reply(&sock->req, ret);
		sock->flags &= ~SOCK_FLG_OP_PENDING;

		if (ret > 0)
			return 0;
	}

	/* Do not enqueue more data than allowed */
	if (sock->recv_data_size > RAW_BUF_SIZE) {
		return 0;
	}

	/*
	 * nobody is waiting for the data or an error occured above, we enqueue
	 * the packet. We store a copy of this packet
	 */
	pbuf_new = pbuf_alloc(PBUF_RAW, pbuf->tot_len, PBUF_RAM);
	if (pbuf_new == NULL) {
		debug_print("LWIP : cannot allocated new pbuf\n");
		return 0;
	}

	if (pbuf_copy(pbuf_new, pbuf) != ERR_OK) {
		debug_print("LWIP : cannot copy pbuf\n");
		return 0;
	}

	/*
	 * If we didn't managed to enqueue the packet we report it as not
	 * consumed
	 */
	if (sock_enqueue_data(sock, pbuf_new, pbuf_new->tot_len) != OK) {
		pbuf_free(pbuf_new);
	}

	return 0;
}

static void nic_pkt_received(struct nic * nic, unsigned int size)
{
	assert(nic->netif.input);

#if 0
	print_pkt((unsigned char *) nic->rx_pbuf->payload, 64 /*nic->rx_pbuf->len */);
#endif
	
	assert(nic->rx_pbuf->tot_len == nic->rx_pbuf->len);
	nic->rx_pbuf->tot_len = nic->rx_pbuf->len = size - ETH_CRC_SIZE;

	nic->netif.input(nic->rx_pbuf, &nic->netif);
	nic->rx_pbuf = NULL;
	driver_setup_read(nic);
}

void driver_request(message * m)
{
	struct nic * nic;

	if ((nic = lookup_nic_by_drv_ep(m->m_source)) == NULL) {
		printf("LWIP : request from unknown driver %d\n", m->m_source);
		return;
	}

	switch (m->m_type) {
	case DL_CONF_REPLY:
		if (m->m_netdrv_net_dl_conf.stat == OK)
			nic_up(nic, m);
		break;
	case DL_TASK_REPLY:
		/*
		if (!(m->m_netdrv_net_dl_task.flags & DL_PACK_SEND) &&
			!(m->m_netdrv_net_dl_task.flags & DL_PACK_RECV)) {
			printf("void reply from driver\n");
			break;
		}
		*/
		if (m->m_netdrv_net_dl_task.flags & DL_PACK_SEND)
			nic_pkt_sent(nic);
		if (m->m_netdrv_net_dl_task.flags & DL_PACK_RECV)
			nic_pkt_received(nic, m->m_netdrv_net_dl_task.count);
		break;
	case DL_STAT_REPLY:
		break;
	default:
		printf("LWIP : unexpected request %d from driver %d\n",
						m->m_type, m->m_source);
	}
}

void driver_up(const char * label, endpoint_t ep)
{
	struct nic * nic;

	nic = lookup_nic_by_drv_name(label);
	
	if (nic) {
		debug_print("LWIP : driver '%s' / %d is up for /dev/%s\n",
				label, ep, nic->name);
		nic->drv_ep = ep;
	} else {
		printf("LWIP : WARNING unexpected driver '%s' up event\n",
								label);
		return;
	}

	nic->state = DRV_IDLE;

	/*
	 * FIXME
	 *
	 * We set the initial ip to 0.0.0.0 to make dhcpd broadcasing work
	 * at the very begining. dhcp should use raw socket but it is a little
	 * tricy in the current dhcp implementation
	 */
	if (!netif_add(&nic->netif, (ip_addr_t *) __UNCONST( &ip_addr_any),
	  &ip_addr_none, &ip_addr_none, nic, ethernetif_init, ethernet_input)) {
		printf("LWIP : failed to add device /dev/%s\n", nic->name);
		nic->drv_ep = NONE;
	}
	if (nic->is_default)
		netif_set_default(&nic->netif);

	/* FIXME we support ethernet only, 2048 is safe */
	nic->tx_buffer = debug_malloc(2048);
	if (nic->tx_buffer == NULL)
		panic("Cannot allocate tx_buffer");
	/* When driver restarts, the rx_pbuf is likely ready to receive data
	 * from its previous instance. We free the buffer here, nobody depends
	 * on it. A new one is allocated when we send a new read request to the
	 * driver.
	 */
	if (nic->rx_pbuf) {
		pbuf_free(nic->rx_pbuf);
		nic->rx_pbuf = NULL;
	}

	/* prepare the RX grant once and forever */
	if (cpf_setgrant_direct(nic->rx_iogrant,
				nic->drv_ep,
				(vir_bytes) &nic->rx_iovec,
				1 * sizeof(iovec_s_t), CPF_READ) != OK)
		panic("Failed to set grant");
}

static void raw_recv_free(__unused void * data)
{
	pbuf_free((struct pbuf *) data);
}

static int nic_op_close(struct socket * sock)
{
	struct nic * nic = (struct nic *)sock->data;

	debug_drv_print("socket %ld", get_sock_num(sock));
	
	sock_dequeue_data_all(sock, raw_recv_free);
	sock->ops = NULL;

	if (nic->raw_socket == sock) {
		nic->raw_socket = NULL;
		debug_drv_print("no active raw sock at %s", nic->name);
	}

	return OK;
}

static int nic_ioctl_set_conf(__unused struct socket * sock,
				struct nic * nic,
				endpoint_t endpt,
				cp_grant_id_t grant)
{
	nwio_ipconf_t ipconf;
	int err;

	err = copy_from_user(endpt, &ipconf, sizeof(ipconf), grant, 0);
	if (err != OK)
		return err;

	if (ipconf.nwic_flags & NWIC_IPADDR_SET)
		netif_set_ipaddr(&nic->netif,
				(ip_addr_t *)&ipconf.nwic_ipaddr);
	if (ipconf.nwic_flags & NWIC_NETMASK_SET)
		netif_set_netmask(&nic->netif,
				(ip_addr_t *)&ipconf.nwic_netmask);
	nic->flags = ipconf.nwic_flags;
	if (nic->flags & NWEO_EN_BROAD)
		nic->netif.flags |= NETIF_FLAG_BROADCAST;
	

	return OK;
}

static int nic_ioctl_get_conf(__unused struct socket * sock,
				struct nic * nic,
				endpoint_t endpt,
				cp_grant_id_t grant)
{
	nwio_ipconf_t ipconf;

	ipconf.nwic_flags = nic->flags;
	ipconf.nwic_ipaddr = nic->netif.ip_addr.addr;
	ipconf.nwic_netmask = nic->netif.netmask.addr;
	ipconf.nwic_mtu = nic->netif.mtu;
	
	return copy_to_user(endpt, &ipconf, sizeof(ipconf), grant, 0);
}

static int nic_ioctl_set_gateway(__unused struct socket * sock,
				struct nic * nic,
				endpoint_t endpt,
				cp_grant_id_t grant)
{
	nwio_route_t route;
	int err;

	err = copy_from_user(endpt, &route, sizeof(route), grant, 0);
	if (err != OK)
		return err;

	netif_set_gw(&nic->netif, (ip_addr_t *)&route.nwr_gateway);

	return OK;
}

static int nic_ioctl_get_ethstat(__unused struct socket * sock,
				struct nic * nic,
				endpoint_t endpt,
				cp_grant_id_t grant)
{
	nwio_ethstat_t ethstat;

	debug_drv_print("device /dev/%s", nic->name);
	/*
	 * The device is not up yet, there is nothing to report or it is not
	 * an ethernet device
	 */
	if (!nic->netif.flags & NETIF_FLAG_UP ||
			!(nic->netif.flags & (NETIF_FLAG_ETHERNET |
				NETIF_FLAG_ETHARP))) {
		printf("LWIP no such device FUCK\n");
		return ENODEV;
	}

	memset(&ethstat, 0, sizeof(ethstat));
	memcpy(&ethstat.nwes_addr, nic->netif.hwaddr, 6);
	
	return copy_to_user(endpt, &ethstat, sizeof(ethstat), grant, 0);
}

static int nic_ioctl_set_ethopt(struct socket * sock,
				struct nic * nic,
				endpoint_t endpt,
				cp_grant_id_t grant)
{
	int err;
	nwio_ethopt_t ethopt;

	assert(nic);

	if (!sock)
		return EINVAL;

	debug_drv_print("device /dev/%s", nic->name);
	/*
	 * The device is not up yet, there is nothing to report or it is not
	 * an ethernet device
	 */
	if (!nic->netif.flags & NETIF_FLAG_UP ||
			!(nic->netif.flags & (NETIF_FLAG_ETHERNET |
				NETIF_FLAG_ETHARP))) {
		return ENODEV;
	}

	err = copy_from_user(endpt, &ethopt, sizeof(ethopt), grant, 0);
	if (err != OK)
		return err;

	/* we want to get data from this sock */
	if (ethopt.nweo_flags & NWEO_COPY) {
		if (nic->raw_socket)
			return EBUSY;

		nic->raw_socket = sock;
		debug_drv_print("active raw sock %ld at %s",
				get_sock_num(sock), nic->name);
	}

	return OK;
}

static int nic_do_ioctl(struct socket * sock, struct nic * nic,
	struct sock_req * req)
{
	int r;

	debug_print("device /dev/%s req %c %ld %ld",
			nic->name,
			(unsigned char) (req->req >> 8),
			req->req & 0xff, _MINIX_IOCTL_SIZE(req->req));
	
	debug_drv_print("socket %ld", sock ? get_sock_num(sock) : -1);

	switch (req->req) {
	case NWIOSIPCONF:
		r = nic_ioctl_set_conf(sock, nic, req->endpt, req->grant);
		break;
	case NWIOGIPCONF:
		r = nic_ioctl_get_conf(sock, nic, req->endpt, req->grant);
		break;
	case NWIOSIPOROUTE:
		r = nic_ioctl_set_gateway(sock, nic, req->endpt, req->grant);
		break;
	case NWIOGETHSTAT:
		r = nic_ioctl_get_ethstat(sock, nic, req->endpt, req->grant);
		break;
	case NWIOSETHOPT:
		r = nic_ioctl_set_ethopt(sock, nic, req->endpt, req->grant);
		break;
	default:
		r = ENOTTY;
	}

	return r;
}

int nic_default_ioctl(struct sock_req *req)
{
	struct nic * nic = lookup_nic_default();

	if (nic == NULL) {
		debug_print("No default nic, reporting error");
		return ENOTTY;
	}

	return nic_do_ioctl(NULL, nic, req);
}

static int nic_op_ioctl(struct socket * sock, struct sock_req * req,
	__unused int blk)
{
	return nic_do_ioctl(sock, (struct nic *)sock->data, req);
}

static int nic_op_read(struct socket * sock, struct sock_req * req, int blk)
{
	debug_drv_print("sock num %ld", get_sock_num(sock));

	if (sock->recv_head) {
		/* data available receive immeditely */

		struct pbuf * pbuf;
		int ret;

		pbuf = sock->recv_head->data;

		ret = raw_receive(req, pbuf);

		if (ret > 0) {
			sock_dequeue_data(sock);
			sock->recv_data_size -= pbuf->tot_len;
			pbuf_free(pbuf);
		}
		return ret;
	} else if (!blk)
		return EAGAIN;
	else {
		/* store the request so we know how to reply */
		sock->req = *req;
		/* operation is being processes */
		sock->flags |= SOCK_FLG_OP_PENDING;

		debug_print("no data to read, suspending");
		return EDONTREPLY;
	}
}

static int nic_op_write(struct socket * sock, struct sock_req * req,
	__unused int blk)
{
	int ret;
	struct pbuf * pbuf;
	struct nic * nic = (struct nic *)sock->data;

	assert(nic);
	debug_print("device %s data size %u", nic->name, req->size);

	pbuf = pbuf_alloc(PBUF_RAW, req->size, PBUF_RAM);
	if (!pbuf)
		return ENOMEM;

	if ((ret = copy_from_user(req->endpt, pbuf->payload, req->size,
			req->grant, 0)) != OK) {
		pbuf_free(pbuf);
		return ret;
	}

	if ((ret = nic->netif.linkoutput(&nic->netif, pbuf) != ERR_OK)) {
		debug_print("raw linkoutput failed %d", ret);
		ret = EIO;
	} else
		ret = req->size;

	pbuf_free(pbuf);

	return ret;
}

static struct sock_ops nic_ops = {
	.write 		= nic_op_write,
	.read 		= nic_op_read,
	.close 		= nic_op_close,
	.ioctl 		= nic_op_ioctl,
	.select		= generic_op_select,
	.select_reply	= generic_op_select_reply
};

int nic_open(devminor_t minor)
{
	struct socket * sock;

	debug_print("device %d", minor);

	if (minor > MAX_DEVS || devices[minor].drv_ep == NONE)
		return ENODEV;

	sock = get_unused_sock();

	if (sock == NULL)
		return ENODEV;
	if (sock->ops != NULL)
		return EBUSY;

	sock->ops = &nic_ops;
	sock->select_ep = NONE;
	sock->recv_data_size = 0;
	sock->data = &devices[minor];

	return get_sock_num(sock);
}

static int driver_pkt_enqueue(struct packet_q ** head,
				struct packet_q ** tail,
				struct pbuf * pbuf)
{
	struct packet_q * pkt;
	char * b;

	pkt = (struct packet_q *) malloc(sizeof(struct packet_q) + pbuf->tot_len);
	if (!pkt)
		return ENOMEM;

	pkt->next = NULL;
	pkt->buf_len = pbuf->tot_len;
	
	for (b = pkt->buf; pbuf; pbuf = pbuf->next) {
		memcpy(b, pbuf->payload, pbuf->len);
		b += pbuf->len;
	}

	if (*head == NULL)
		*head = *tail = pkt;
	else {
		(*tail)->next = pkt;
		*tail = pkt;
	}

	return OK;
}

int driver_tx_enqueue(struct nic * nic, struct pbuf * pbuf)
{
	debug_print("device /dev/%s", nic->name);
	return driver_pkt_enqueue(&nic->tx_head, &nic->tx_tail, pbuf);
}

static void driver_pkt_dequeue(struct packet_q ** head,
					struct packet_q ** tail)
{
	struct packet_q * pkt;

	/* we always dequeue only if there is something to dequeue */
	assert(*head);

	pkt = *head;

	if ((*head = pkt->next) == NULL)
		*tail = NULL;

	debug_free(pkt);
}

void driver_tx_dequeue(struct nic * nic)
{
	debug_print("device /dev/%s", nic->name);
	driver_pkt_dequeue(&nic->tx_head, &nic->tx_tail);
}

struct packet_q * driver_tx_head(struct nic * nic)
{
	debug_print("device /dev/%s", nic->name);

	if (!nic->tx_head)
		return NULL;
	return nic->tx_head;
}
