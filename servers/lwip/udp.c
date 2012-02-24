#include <stdlib.h>

#include <minix/sysutil.h>

#include <sys/ioc_net.h>
#include <net/gen/in.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <net/gen/udp_io_hdr.h>

#include <lwip/udp.h>
#include <lwip/ip_addr.h>

#include <minix/netsock.h>
#include "proto.h"

#define UDP_BUF_SIZE	(4 << 10)

#define sock_alloc_buf(s)	debug_malloc(s)
#define sock_free_buf(x)	debug_free(x)

#if 0
#define debug_udp_print(str, ...) printf("LWIP %s:%d : " str "\n", \
		__func__, __LINE__, ##__VA_ARGS__)
#else
#define debug_udp_print(...) debug_print(__VA_ARGS__)
#endif

struct udp_recv_data {
	ip_addr_t	ip;
	u16_t		port;
	struct pbuf *	pbuf;
};

#define udp_recv_alloc()	debug_malloc(sizeof(struct udp_recv_data))

static void udp_recv_free(void * data)
{
	if (((struct udp_recv_data *)data)->pbuf)
		pbuf_free(((struct udp_recv_data *)data)->pbuf);
	debug_free(data);
}

static int udp_op_open(struct socket * sock, __unused message * m)
{
	struct udp_pcb * pcb;

	debug_udp_print("socket num %ld", get_sock_num(sock));

	if (!(pcb = udp_new()))
		return ENOMEM;

	sock->buf = NULL;
	sock->buf_size = 0;
	
	sock->pcb = pcb;
	
	return OK;
}

static void udp_op_close(struct socket * sock, __unused message * m)
{
	debug_udp_print("socket num %ld", get_sock_num(sock));

	/* deque and free all enqueued data before closing */
	sock_dequeue_data_all(sock, udp_recv_free);

	if (sock->pcb)
		udp_remove(sock->pcb);
	assert(sock->buf == NULL);

	/* mark it as unused */
	sock->ops = NULL;

	sock_reply_close(sock, OK);
}

static int udp_do_receive(struct socket * sock,
			message * m,
			struct udp_pcb *pcb,
			struct pbuf *pbuf,
			ip_addr_t *addr,
			u16_t port)
{
	struct pbuf * p;
	unsigned rem_len = m->COUNT;
	unsigned written = 0, hdr_sz = 0;
	int err;

	debug_udp_print("user buffer size : %d", rem_len);

	/* FIXME make it both a single copy */
	if (!(sock->usr_flags & NWUO_RWDATONLY)) {
		udp_io_hdr_t hdr;

		hdr.uih_src_addr = addr->addr;
		hdr.uih_src_port = htons(port);
		hdr.uih_dst_addr = pcb->local_ip.addr;
		hdr.uih_dst_port = htons(pcb->local_port);

		hdr.uih_data_len = 0;
		hdr.uih_ip_opt_len = 0;

		err = copy_to_user(m->m_source,
				&hdr, sizeof(hdr),
				(cp_grant_id_t) m->IO_GRANT,
				0);

		if (err != OK)
			return err;

		rem_len -= (hdr_sz = sizeof(hdr));
	}

	for (p = pbuf; p && rem_len; p = p->next) {
		size_t cp_len;

		cp_len = (rem_len < p->len) ? rem_len : p->len;
		err = copy_to_user(m->m_source,	p->payload, cp_len,
				(cp_grant_id_t) m->IO_GRANT,
				hdr_sz + written);

		if (err != OK)
			return err;

		written += cp_len;
		rem_len -= cp_len;
	}

	debug_udp_print("copied %d bytes", written + hdr_sz);
	return written + hdr_sz;
}

static void udp_recv_callback(void *arg,
			struct udp_pcb *pcb,
			struct pbuf *pbuf,
			ip_addr_t *addr,
			u16_t port)
{
	struct socket * sock = (struct socket *) arg;
	struct udp_recv_data * data;

	debug_udp_print("socket num : %ld addr : %x port : %d\n",
			get_sock_num(sock), (unsigned int) addr->addr, port);

	if (sock->flags & SOCK_FLG_OP_PENDING) {
		/* we are resuming a suspended operation */
		int ret;

		ret = udp_do_receive(sock, &sock->mess, pcb, pbuf, addr, port);

		if (ret > 0) {
			pbuf_free(pbuf);
			sock_reply(sock, ret);
			sock->flags &= ~SOCK_FLG_OP_PENDING;
			return;
		} else {
			sock_reply(sock, ret);
			sock->flags &= ~SOCK_FLG_OP_PENDING;
		}
	}

	/* Do not enqueue more data than allowed */
	if (sock->recv_data_size > UDP_BUF_SIZE) {
		pbuf_free(pbuf);
		return;
	}

	/*
	 * nobody is waiting for the data or an error occured above, we enqueue
	 * the packet
	 */
	if (!(data = udp_recv_alloc())) {
		pbuf_free(pbuf);
		return;
	}

	data->ip = *addr;
	data->port = port;
	data->pbuf = pbuf;

	if (sock_enqueue_data(sock, data, data->pbuf->tot_len) != OK) {
		udp_recv_free(data);
		return;
	}
	
	/*
	 * We don't need to notify when somebody is already waiting, reviving
	 * read operation will do the trick for us. But we must announce new
	 * data available here.
	 */
	if (sock_select_read_set(sock))
		sock_select_notify(sock);
}

static void udp_op_read(struct socket * sock, message * m, int blk)
{
	debug_udp_print("socket num %ld", get_sock_num(sock));

	if (sock->recv_head) {
		/* data available receive immeditely */

		struct udp_recv_data * data;
		int ret;

		data = (struct udp_recv_data *) sock->recv_head->data;

		ret = udp_do_receive(sock, m, (struct udp_pcb *) sock->pcb,
					data->pbuf, &data->ip, data->port);

		if (ret > 0) {
			sock_dequeue_data(sock);
			sock->recv_data_size -= data->pbuf->tot_len;
			udp_recv_free(data);
		}
		sock_reply(sock, ret);
	} else if (!blk)
		sock_reply(sock, EAGAIN);
	else {
		/* store the message so we know how to reply */
		sock->mess = *m;
		/* operation is being processes */
		sock->flags |= SOCK_FLG_OP_PENDING;

		debug_udp_print("no data to read, suspending\n");
	}
}

static int udp_op_send(struct socket * sock,
			struct pbuf * pbuf,
			message * m)
{
	int err;

	debug_udp_print("pbuf len %d\n", pbuf->len);

	if ((err = udp_send(sock->pcb, pbuf)) == ERR_OK)
		return m->COUNT;
	else {
		debug_udp_print("udp_send failed %d", err);
		return EIO;
	}
}

static int udp_op_sendto(struct socket * sock, struct pbuf * pbuf, message * m)
{
	int err;
	udp_io_hdr_t hdr;

	hdr = *(udp_io_hdr_t *) pbuf->payload;

	pbuf_header(pbuf, -(s16_t)sizeof(udp_io_hdr_t));

	debug_udp_print("data len %d pbuf len %d\n",
			hdr.uih_data_len, pbuf->len);

	if ((err = udp_sendto(sock->pcb, pbuf, (ip_addr_t *) &hdr.uih_dst_addr,
						ntohs(hdr.uih_dst_port))) == ERR_OK)
		return m->COUNT;
	else {
		debug_udp_print("udp_sendto failed %d", err);
		return EIO;
	}
}

static void udp_op_write(struct socket * sock, message * m, __unused int blk)
{
	int ret;
	struct pbuf * pbuf;

	debug_udp_print("socket num %ld data size %d",
			get_sock_num(sock), m->COUNT);

	pbuf = pbuf_alloc(PBUF_TRANSPORT, m->COUNT, PBUF_POOL);
	if (!pbuf) {
		ret = ENOMEM;
		goto write_err;
	}

	if ((ret = copy_from_user(m->m_source, pbuf->payload, m->COUNT,
				(cp_grant_id_t) m->IO_GRANT, 0)) != OK) {
		pbuf_free(pbuf);
		goto write_err;
	}

	if (sock->usr_flags & NWUO_RWDATONLY)
		ret = udp_op_send(sock, pbuf, m);
	else
		ret = udp_op_sendto(sock, pbuf, m);

	if (pbuf_free(pbuf) == 0) {
		panic("We cannot buffer udp packets yet!");
	}
	
write_err:
	sock_reply(sock, ret);
}

static void udp_set_opt(struct socket * sock, message * m)
{
	int err;
	nwio_udpopt_t udpopt;
	struct udp_pcb * pcb = (struct udp_pcb *) sock->pcb;
	ip_addr_t loc_ip = ip_addr_any;

	assert(pcb);

	err = copy_from_user(m->m_source, &udpopt, sizeof(udpopt),
				(cp_grant_id_t) m->IO_GRANT, 0);

	if (err != OK)
		sock_reply(sock, err);

	debug_udp_print("udpopt.nwuo_flags = 0x%lx", udpopt.nwuo_flags);
	debug_udp_print("udpopt.nwuo_remaddr = 0x%x",
				(unsigned int) udpopt.nwuo_remaddr);
	debug_udp_print("udpopt.nwuo_remport = 0x%x",
				ntohs(udpopt.nwuo_remport));
	debug_udp_print("udpopt.nwuo_locaddr = 0x%x",
				(unsigned int) udpopt.nwuo_locaddr);
	debug_udp_print("udpopt.nwuo_locport = 0x%x",
				ntohs(udpopt.nwuo_locport));

	sock->usr_flags = udpopt.nwuo_flags;

	/*
	 * We will only get data from userspace and the remote address
	 * and port are being set which means that from now on we must
	 * know where to send data. Thus we should interpret this as
	 * connect() call
	 */
	if (sock->usr_flags & NWUO_RWDATONLY &&
			sock->usr_flags & NWUO_RP_SET &&
			sock->usr_flags & NWUO_RA_SET)
		udp_connect(pcb, (ip_addr_t *) &udpopt.nwuo_remaddr,
						ntohs(udpopt.nwuo_remport));
	/* Setting local address means binding */
	if (sock->usr_flags & NWUO_LP_SET)
		udp_bind(pcb, &loc_ip, ntohs(udpopt.nwuo_locport));
	/* We can only bind to random local port */
	if (sock->usr_flags & NWUO_LP_SEL)
		udp_bind(pcb, &loc_ip, 0);

	
	/* register a receive hook */
	udp_recv((struct udp_pcb *) sock->pcb, udp_recv_callback, sock);

	sock_reply(sock, OK);
}

static void udp_get_opt(struct socket * sock, message * m)
{
	int err;
	nwio_udpopt_t udpopt;
	struct udp_pcb * pcb = (struct udp_pcb *) sock->pcb;

	assert(pcb);

	udpopt.nwuo_locaddr = pcb->local_ip.addr;
	udpopt.nwuo_locport = htons(pcb->local_port);
	udpopt.nwuo_remaddr = pcb->remote_ip.addr;
	udpopt.nwuo_remport = htons(pcb->remote_port);
	udpopt.nwuo_flags = sock->usr_flags;

	debug_udp_print("udpopt.nwuo_flags = 0x%lx", udpopt.nwuo_flags);
	debug_udp_print("udpopt.nwuo_remaddr = 0x%x",
				(unsigned int) udpopt.nwuo_remaddr);
	debug_udp_print("udpopt.nwuo_remport = 0x%x",
				ntohs(udpopt.nwuo_remport));
	debug_udp_print("udpopt.nwuo_locaddr = 0x%x",
				(unsigned int) udpopt.nwuo_locaddr);
	debug_udp_print("udpopt.nwuo_locport = 0x%x",
				ntohs(udpopt.nwuo_locport));

	if ((unsigned) m->COUNT < sizeof(udpopt)) {
		sock_reply(sock, EINVAL);
		return;
	}

	err = copy_to_user(m->m_source, &udpopt, sizeof(udpopt),
				(cp_grant_id_t) m->IO_GRANT, 0);

	if (err != OK)
		sock_reply(sock, err);

	sock_reply(sock, OK);
}

static void udp_op_ioctl(struct socket * sock, message * m, __unused int blk)
{
	debug_udp_print("socket num %ld req %c %d %d",
			get_sock_num(sock),
			(m->REQUEST >> 8) & 0xff,
			m->REQUEST & 0xff,
			(m->REQUEST >> 16) & _IOCPARM_MASK);

	switch (m->REQUEST) {
	case NWIOSUDPOPT:
		udp_set_opt(sock, m);
		break;
	case NWIOGUDPOPT:
		udp_get_opt(sock, m);
		break;
	default:
		sock_reply(sock, EBADIOCTL);
		return;
	}
}

struct sock_ops sock_udp_ops = {
	.open		= udp_op_open,
	.close		= udp_op_close,
	.read		= udp_op_read,
	.write		= udp_op_write,
	.ioctl		= udp_op_ioctl,
	.select		= generic_op_select,
	.select_reply	= generic_op_select_reply
};

