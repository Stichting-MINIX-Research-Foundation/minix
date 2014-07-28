#include <stdlib.h>

#include <sys/ioc_net.h>
#include <net/gen/in.h>
#include <net/gen/ip_io.h>

#include <lwip/raw.h>
#include <lwip/ip_addr.h>

#include <minix/netsock.h>
#include "proto.h"

#define RAW_IP_BUF_SIZE	(32 << 10)

#define sock_alloc_buf(s)	debug_malloc(s)
#define sock_free_buf(x)	debug_free(x)

struct raw_ip_recv_data {
	ip_addr_t	ip;
	struct pbuf *	pbuf;
};

#define raw_ip_recv_alloc()	debug_malloc(sizeof(struct raw_ip_recv_data))

static void raw_ip_recv_free(void * data)
{
	if (((struct raw_ip_recv_data *)data)->pbuf)
		pbuf_free(((struct raw_ip_recv_data *)data)->pbuf);
	debug_free(data);
}


static int raw_ip_op_open(struct socket * sock)
{
	debug_print("socket num %ld", get_sock_num(sock));

	if (!(sock->buf = sock_alloc_buf(RAW_IP_BUF_SIZE))) {
		return ENOMEM;
	}
	sock->buf_size = RAW_IP_BUF_SIZE;

	return OK;
}

static void raw_ip_close(struct socket * sock)
{
	/* deque and free all enqueued data before closing */
	sock_dequeue_data_all(sock, raw_ip_recv_free);

	if (sock->pcb)
		raw_remove(sock->pcb);
	if (sock->buf)
		sock_free_buf(sock->buf);

	/* mark it as unused */
	sock->ops = NULL;
}

static int raw_ip_op_close(struct socket * sock)
{
	debug_print("socket num %ld", get_sock_num(sock));

	raw_ip_close(sock);

	return OK;
}

static int raw_ip_do_receive(struct sock_req *req,
			struct pbuf *pbuf)
{
	struct pbuf * p;
	size_t rem_len = req->size;
	unsigned int written = 0, hdr_sz = 0;
	int err;

	debug_print("user buffer size : %u\n", rem_len);

	for (p = pbuf; p && rem_len; p = p->next) {
		size_t cp_len;

		cp_len = (rem_len < p->len) ? rem_len : p->len;
		err = copy_to_user(req->endpt, p->payload, cp_len, req->grant,
				hdr_sz + written);

		if (err != OK)
			return err;

		written += cp_len;
		rem_len -= cp_len;
	}

	debug_print("copied %d bytes\n", written + hdr_sz);
	return written + hdr_sz;
}

static u8_t raw_ip_op_receive(void *arg,
			__unused struct raw_pcb *pcb,
			struct pbuf *pbuf,
			ip_addr_t *addr)
{
	struct socket * sock = (struct socket *) arg;
	struct raw_ip_recv_data * data;
	int ret;

	debug_print("socket num : %ld addr : %x\n",
			get_sock_num(sock), (unsigned int) addr->addr);

	if (sock->flags & SOCK_FLG_OP_PENDING) {
		/* we are resuming a suspended operation */
		ret = raw_ip_do_receive(&sock->req, pbuf);

		send_req_reply(&sock->req, ret);
		sock->flags &= ~SOCK_FLG_OP_PENDING;

		if (ret > 0) {
			if (sock->usr_flags & NWIO_EXCL) {
				pbuf_free(pbuf);
				return 1;
			} else
				return 0;
		}
	}

	/* Do not enqueue more data than allowed */
	if (sock->recv_data_size > RAW_IP_BUF_SIZE)
		return 0;

	/*
	 * nobody is waiting for the data or an error occured above, we enqueue
	 * the packet
	 */
	if (!(data = raw_ip_recv_alloc())) {
		return 0;
	}

	data->ip = *addr;
	if (sock->usr_flags & NWIO_EXCL) {
		data->pbuf = pbuf;
		ret = 1;
	} else {
		/* we store a copy of this packet */
		data->pbuf = pbuf_alloc(PBUF_RAW, pbuf->tot_len, PBUF_RAM);
		if (data->pbuf == NULL) {
			debug_print("LWIP : cannot allocated new pbuf\n");
			raw_ip_recv_free(data);
			return 0;
		}

		if (pbuf_copy(data->pbuf, pbuf) != ERR_OK) {
			debug_print("LWIP : cannot copy pbuf\n");
			raw_ip_recv_free(data);
			return 0;
		}

		ret = 0;
	}

	/*
	 * If we didn't managed to enqueue the packet we report it as not
	 * consumed
	 */
	if (sock_enqueue_data(sock, data, data->pbuf->tot_len) != OK) {
		raw_ip_recv_free(data);
		ret = 0;
	}

	return ret;
}

static int raw_ip_op_read(struct socket * sock, struct sock_req * req, int blk)
{
	debug_print("socket num %ld", get_sock_num(sock));

	if (sock->pcb == NULL)
		return EIO;

	if (sock->recv_head) {
		/* data available receive immeditely */

		struct raw_ip_recv_data * data;
		int ret;

		data = (struct raw_ip_recv_data *) sock->recv_head->data;

		ret = raw_ip_do_receive(req, data->pbuf);

		if (ret > 0) {
			sock_dequeue_data(sock);
			sock->recv_data_size -= data->pbuf->tot_len;
			raw_ip_recv_free(data);
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

static int raw_ip_op_write(struct socket * sock, struct sock_req * req,
	__unused int blk)
{
	int ret;
	struct pbuf * pbuf;
	struct ip_hdr * ip_hdr;

	debug_print("socket num %ld data size %u",
			get_sock_num(sock), req->size);

	if (sock->pcb == NULL)
		return EIO;

	if (req->size > sock->buf_size)
		return ENOMEM;

	pbuf = pbuf_alloc(PBUF_LINK, req->size, PBUF_RAM);
	if (!pbuf)
		return ENOMEM;

	if ((ret = copy_from_user(req->endpt, pbuf->payload, req->size,
				  req->grant, 0)) != OK) {
		pbuf_free(pbuf);
		return ret;
	}

	ip_hdr = (struct ip_hdr *) pbuf->payload;
	if (pbuf_header(pbuf, -IP_HLEN)) {
		pbuf_free(pbuf);
		return EIO;
	}

	if ((ret = raw_sendto((struct raw_pcb *)sock->pcb, pbuf,
				(ip_addr_t *) &ip_hdr->dest)) != OK) {
		debug_print("raw_sendto failed %d", ret);
		ret = EIO;
	} else
		ret = req->size;
	

	pbuf_free(pbuf);
	
	return ret;
}

static int raw_ip_set_opt(struct socket * sock, endpoint_t endpt,
	cp_grant_id_t grant)
{
	int err;
	nwio_ipopt_t ipopt;
	struct raw_pcb * pcb;

	err = copy_from_user(endpt, &ipopt, sizeof(ipopt), grant, 0);

	if (err != OK)
		return err;

	debug_print("ipopt.nwio_flags = 0x%x", ipopt.nwio_flags);
	debug_print("ipopt.nwio_proto = 0x%x", ipopt.nwio_proto);
	debug_print("ipopt.nwio_rem = 0x%x",
				(unsigned int) ipopt.nwio_rem);

	if (sock->pcb == NULL) {
		if (!(pcb = raw_new(ipopt.nwio_proto))) {
			raw_ip_close(sock);
			return ENOMEM;
		}

		sock->pcb = pcb;
	} else
		pcb = (struct raw_pcb *) sock->pcb;

	if (pcb->protocol != ipopt.nwio_proto) {
		debug_print("conflicting ip socket protocols\n");
		return EINVAL;
	}

	sock->usr_flags = ipopt.nwio_flags;

#if 0
	if (raw_bind(pcb, (ip_addr_t *)&ipopt.nwio_rem) == ERR_USE) {
		raw_ip_close(sock);
		return EADDRINUSE;
	}
#endif

	/* register a receive hook */
	raw_recv((struct raw_pcb *) sock->pcb, raw_ip_op_receive, sock);

	return OK;
}

static int raw_ip_get_opt(struct socket * sock, endpoint_t endpt,
	cp_grant_id_t grant)
{
	nwio_ipopt_t ipopt;
	struct raw_pcb * pcb = (struct raw_pcb *) sock->pcb;

	assert(pcb);

	ipopt.nwio_rem = pcb->remote_ip.addr;
	ipopt.nwio_flags = sock->usr_flags;

	return copy_to_user(endpt, &ipopt, sizeof(ipopt), grant, 0);
}

static int raw_ip_op_ioctl(struct socket * sock, struct sock_req * req,
	__unused int blk)
{
	int r;

	debug_print("socket num %ld req %c %ld %ld",
			get_sock_num(sock),
			(unsigned char) (req->req >> 8),
			req->req & 0xff,
			_MINIX_IOCTL_SIZE(req->req));
	
	switch (req->req) {
	case NWIOSIPOPT:
		r = raw_ip_set_opt(sock, req->endpt, req->grant);
		break;
	case NWIOGIPOPT:
		r = raw_ip_get_opt(sock, req->endpt, req->grant);
		break;
	default:
		/*
		 * /dev/ip can be also accessed as a default device to be
		 * configured
		 */
		r = nic_default_ioctl(req);
	}

	return r;
}

struct sock_ops sock_raw_ip_ops = {
	.open		= raw_ip_op_open,
	.close		= raw_ip_op_close,
	.read		= raw_ip_op_read,
	.write		= raw_ip_op_write,
	.ioctl		= raw_ip_op_ioctl,
	.select		= generic_op_select,
	.select_reply	= generic_op_select_reply
};
