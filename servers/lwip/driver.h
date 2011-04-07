#ifndef __LWIP_DRIVER_H_
#define __LWIP_DRIVER_H_

#include <minix/endpoint.h>
#include <minix/ds.h>

#include <lwip/pbuf.h>

#define NIC_NAME_LEN	6
#define DRV_NAME_LEN	DS_MAX_KEYLEN

#define TX_IOVEC_NUM	16 /* something the drivers assume */

struct packet_q {
	struct packet_q *	next;
	unsigned		buf_len;
	char			buf[];
};

#define DRV_IDLE	0
#define DRV_SENDING	1
#define DRV_RECEIVING	2

struct nic {
	unsigned		flags;
	char			name[NIC_NAME_LEN];
	char			drv_name[DRV_NAME_LEN];
	endpoint_t		drv_ep;
	int			is_default;
	int			state;
	cp_grant_id_t		rx_iogrant;
	iovec_s_t		rx_iovec[1];
	struct pbuf *		rx_pbuf;
	cp_grant_id_t		tx_iogrant;
	iovec_s_t		tx_iovec[TX_IOVEC_NUM];
	struct packet_q	*	tx_head;
	struct packet_q	*	tx_tail;
	void *			tx_buffer;
	struct netif		netif;
	unsigned		max_pkt_sz;
	unsigned		min_pkt_sz;
	struct socket		* raw_socket;
};

int driver_tx_enqueue(struct nic * nic, struct pbuf * pbuf);
void driver_tx_dequeue(struct nic * nic);
struct packet_q * driver_tx_head(struct nic * nic);

/*
 * Transmit the next packet in the TX queue of this device. Returns 1 if
 * success, 0 otherwise.
 */
int driver_tx(struct nic * nic);
int raw_socket_input(struct pbuf * pbuf, struct nic * nic);

#endif /* __LWIP_DRIVER_H_ */
