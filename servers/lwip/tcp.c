#include <stdlib.h>
#include <assert.h>
#include <minix/sysutil.h>

#include <sys/ioc_net.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#include <lwip/tcp.h>
#include <lwip/tcp_impl.h>
#include <lwip/ip_addr.h>

#include <minix/netsock.h>
#include "proto.h"

#define TCP_BUF_SIZE	(32 << 10)

#define sock_alloc_buf(s)	debug_malloc(s)
#define sock_free_buf(x)	debug_free(x)

static int do_tcp_debug;

#if 0
#define debug_tcp_print(str, ...) printf("LWIP %s:%d : " str "\n", \
		__func__, __LINE__, ##__VA_ARGS__)
#else
#define debug_tcp_print(...) debug_print(__VA_ARGS__)
#endif

struct wbuf {
	unsigned int	len;
	unsigned int	written;
	unsigned int	unacked;
	unsigned int	rem_len;
	struct wbuf	* next;
	char 		data[];
};

struct wbuf_chain {
	struct wbuf * head;
	struct wbuf * tail;
	struct wbuf * unsent; /* points to the first buffer that contains unsent
				 data. It may point anywhere between head and
				 tail */
};

static void tcp_error_callback(void *arg, err_t err)
{
	int perr;
	struct socket * sock = (struct socket *) arg;

	debug_tcp_print("socket num %ld err %d", get_sock_num(sock), err);

	switch (err) {
	case ERR_RST:
		perr = ECONNREFUSED;
		break;
	case ERR_CLSD:
		perr = EPIPE;
		break;
	case ERR_CONN:
		perr = ENOTCONN;
		break;
	default:
		perr = EIO;
	}
	
	if (sock->flags & SOCK_FLG_OP_PENDING) {
		sock_reply(sock, perr);
		sock->flags &= ~SOCK_FLG_OP_PENDING;
	} else if (sock_select_set(sock))
		sock_select_notify(sock);
	/*
	 * When error callback is called the tcb either does not exist anymore
	 * or is going to be deallocated soon after. We must not use the pcb
	 * anymore
	 */
	sock->pcb = NULL;
}

static int tcp_fill_new_socket(struct socket * sock, struct tcp_pcb * pcb)
{
	struct wbuf_chain * wc;

	if (!(wc = malloc(sizeof(struct wbuf_chain))))
		return ENOMEM;
	
	wc-> head = wc->tail = wc->unsent = NULL;
	sock->buf = wc;
	sock->buf_size = 0;
	
	sock->pcb = pcb;
	tcp_arg(pcb, sock);
	tcp_err(pcb, tcp_error_callback);
	tcp_nagle_disable(pcb);

	return OK;
}

static int tcp_op_open(struct socket * sock, __unused message * m)
{
	struct tcp_pcb * pcb;
	int ret;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	if (!(pcb = tcp_new()))
		return ENOMEM;
	debug_tcp_print("new tcp pcb %p\n", pcb);
	
	if ((ret = tcp_fill_new_socket(sock, pcb) != OK))
		tcp_abandon(pcb, 0);
	
	return ret;
}

static void tcp_recv_free(__unused void * data)
{
	pbuf_free((struct pbuf *) data);
}

static void tcp_backlog_free(void * data)
{
	tcp_abort((struct tcp_pcb *) data);
}

static void free_wbuf_chain(struct wbuf_chain * wc)
{
	struct wbuf * wb;

	assert(wc != NULL);

	wb = wc->head;
	while (wb) {
		struct wbuf * w = wb;
		debug_tcp_print("freeing wbuf %p", wb);
		wb = wb->next;
		debug_free(w);
	}

	debug_free(wc);
}

static void tcp_op_close(struct socket * sock, __unused message * m)
{
	debug_tcp_print("socket num %ld", get_sock_num(sock));

	if (sock->flags & SOCK_FLG_OP_LISTENING)
		sock_dequeue_data_all(sock, tcp_backlog_free);
	else
		sock_dequeue_data_all(sock, tcp_recv_free);
	debug_tcp_print("dequed RX data");

	if (sock->pcb) {
		int err;

		/* we are not able to handle any callback anymore */
		if (((struct tcp_pcb *)sock->pcb)->state != LISTEN) {
			tcp_arg((struct tcp_pcb *)sock->pcb, NULL);
			tcp_err((struct tcp_pcb *)sock->pcb, NULL);
			tcp_sent((struct tcp_pcb *)sock->pcb, NULL);
			tcp_recv((struct tcp_pcb *)sock->pcb, NULL);
		}

		err = tcp_close(sock->pcb);
		assert(err == ERR_OK);
		sock->pcb = NULL;
	}
	debug_tcp_print("freed pcb");

	if (sock->buf) {
		free_wbuf_chain((struct wbuf_chain *) sock->buf);
		sock->buf = NULL;
	}
	debug_tcp_print("freed TX data");

	sock_reply_close(sock, OK);
	debug_tcp_print("socket unused");

	/* mark it as unused */
	sock->ops = NULL;
}

__unused static void print_tcp_payload(unsigned char * buf, int len)
{
	int i;

	printf("LWIP tcp payload (%d) :\n", len);
	for (i = 0; i < len; i++, buf++) {
		printf("%02x ", buf[0]);
		if (i % 8 == 7)
			kputc('\n');
	}
	kputc('\n');
}

static int read_from_tcp(struct socket * sock, message * m)
{
	unsigned int rem_buf, written = 0;
	struct pbuf * p;

	assert(!(sock->flags & SOCK_FLG_OP_LISTENING) && sock->recv_head);

	rem_buf = m->COUNT;

	debug_tcp_print("socket num %ld recv buff sz %d", get_sock_num(sock), rem_buf);

	p = (struct pbuf *)sock->recv_head->data;
	while (rem_buf) {
		int err;

		if (rem_buf >= p->len) {
			struct pbuf * np;

			/*
			 * FIXME perhaps copy this to a local buffer and do a
			 * single copy to user then
			 */
#if 0
			print_tcp_payload(p->payload, p->len);
#endif
			err = copy_to_user(m->m_source, p->payload, p->len,
					(cp_grant_id_t) m->IO_GRANT, written);
			if (err != OK)
				goto cp_error;
			sock->recv_data_size -= p->len;

			debug_tcp_print("whole pbuf copied (%d bytes)", p->len);
			rem_buf -= p->len;
			written += p->len;

			if ((np = p->next)) {
				pbuf_ref(np);
				if (pbuf_free(p) != 1)
					panic("LWIP : pbuf_free != 1");
				/*
				 * Mark where we are going to continue if an
				 * error occurs
				 */
				sock->recv_head->data = np;
				p = np;
			} else {
				sock_dequeue_data(sock);
				pbuf_free(p);
				if (sock->recv_head)
					p = (struct pbuf *)sock->recv_head->data;
				else
					break;
			}

			if (rem_buf == 0)
				break;
		} else {
			/*
			 * It must be PBUF_RAM for us to be able to shift the
			 * payload pointer
			 */
			assert(p->type == PBUF_RAM);
			
#if 0
			print_tcp_payload(p->payload, rem_buf);
#endif
			err = copy_to_user(m->m_source, p->payload, rem_buf,
					(cp_grant_id_t) m->IO_GRANT, written);
			if (err != OK)
				goto cp_error;
			sock->recv_data_size -= rem_buf;

			debug_tcp_print("partial pbuf copied (%d bytes)", rem_buf);
			/*
			 * The whole pbuf hasn't been copied out, we only shift
			 * the payload pointer to remember where to continue
			 * next time
			 */
			pbuf_header(p, -rem_buf);
			written += rem_buf;
			break;
		}
	}

	debug_tcp_print("%d bytes written to userspace", written);
	//printf("%d wr, queue %d\n", written, sock->recv_data_size);
	tcp_recved((struct tcp_pcb *) sock->pcb, written);
	return written;

cp_error:
	if (written) {
		debug_tcp_print("%d bytes written to userspace", written);
		return written;
	} else
		return EFAULT;
}

static void tcp_op_read(struct socket * sock, message * m, int blk)
{
	debug_tcp_print("socket num %ld", get_sock_num(sock));

	if (!sock->pcb || ((struct tcp_pcb *) sock->pcb)->state !=
							ESTABLISHED) {
		debug_tcp_print("Connection not established\n");
		sock_reply(sock, ENOTCONN);
		return;
	}
	if (sock->recv_head) {
		/* data available receive immeditely */
		int ret = read_from_tcp(sock,  m);
		debug_tcp_print("read op finished");
		sock_reply(sock, ret);
	} else {
		if (sock->flags & SOCK_FLG_CLOSED) {
			printf("socket %ld already closed!!! call from %d\n",
					get_sock_num(sock), m->USER_ENDPT);
			do_tcp_debug = 1;
			sock_reply(sock, 0);
			return;
		}
                if (!blk) {
                        debug_tcp_print("reading would block -> EAGAIN");
                        sock_reply(sock, EAGAIN);
                        return;
                }
		/* operation is being processed */
		debug_tcp_print("no data to read, suspending");
		sock->flags |= SOCK_FLG_OP_PENDING | SOCK_FLG_OP_READING;
	}
}

static struct wbuf * wbuf_add(struct socket * sock, unsigned int sz)
{
	struct wbuf * wbuf;
	struct wbuf_chain * wc = (struct wbuf_chain *)sock->buf;

	assert(wc);

	wbuf = debug_malloc(sizeof(struct wbuf) + sz);
	if (!wbuf)
		return NULL;
	
	wbuf->len = sz;
	wbuf->written = wbuf->unacked = 0;
	wbuf->next = NULL;

	if (wc->head == NULL)
		wc->head = wc->tail = wbuf;
	else {
		wc->tail->next = wbuf;
		wc->tail = wbuf;
	}

	sock->buf_size += sz;
	debug_tcp_print("buffer %p size %d\n", wbuf, sock->buf_size);

	return wbuf;
}

static struct wbuf * wbuf_ack_sent(struct socket * sock, unsigned int sz)
{
	struct wbuf_chain * wc = (struct wbuf_chain *) sock->buf;
	struct wbuf ** wb;

	wb = &wc->head;
	while (sz && *wb) {
		if ((*wb)->unacked <= sz) {
			struct wbuf * w;
			assert((*wb)->rem_len == 0);
			w = *wb;
			*wb = w->next;
			sock->buf_size -= w->len;
			sz -= w->unacked;
			debug_tcp_print("whole buffer acked (%d / %d), removed",
					w->unacked, w->len);
			debug_free(w);
		} else {
			(*wb)->unacked -= sz;
			(*wb)->written += sz;
			debug_tcp_print("acked %d / %d bytes", sz, (*wb)->len);
			sz = 0;
		}
	}

	/* did we write out more than we had? */
	assert(sz == 0);

	if (wc->head == NULL)
		wc->tail = NULL;
	debug_tcp_print("buffer size %d\n", sock->buf_size);

	return wc->head;
}

static void tcp_op_write(struct socket * sock, message * m, __unused int blk)
{
	int ret;
	struct wbuf * wbuf;
	unsigned int snd_buf_len, usr_buf_len;
	u8_t flgs = 0;


	if (!sock->pcb) {
		sock_reply(sock, ENOTCONN);
		return;
	}

	usr_buf_len = m->COUNT;
	debug_tcp_print("socket num %ld data size %d",
			get_sock_num(sock), usr_buf_len);

	/*
	 * Let at most one buffer grow beyond TCP_BUF_SIZE. This is to minimize
	 * small writes from userspace if only a few bytes were sent before
	 */
	if (sock->buf_size >= TCP_BUF_SIZE) {
		/* FIXME do not block for now */
		debug_tcp_print("WARNING : tcp buffers too large, cannot allocate more");
		sock_reply(sock, ENOMEM);
		return;
	}
	/*
	 * Never let the allocated buffers grow more than to 2xTCP_BUF_SIZE and
	 * never copy more than space available
	 */
	usr_buf_len = (usr_buf_len > TCP_BUF_SIZE ? TCP_BUF_SIZE : usr_buf_len);
	wbuf = wbuf_add(sock, usr_buf_len);
	debug_tcp_print("new wbuf for %d bytes", wbuf->len);
	
	if (!wbuf) {
		debug_tcp_print("cannot allocate new buffer of %d bytes", usr_buf_len);
		sock_reply(sock, ENOMEM);
	}

	if ((ret = copy_from_user(m->m_source, wbuf->data, usr_buf_len,
				(cp_grant_id_t) m->IO_GRANT, 0)) != OK) {
		sock_reply(sock, ret);
		return;
	}

	wbuf->written = 0;
	wbuf->rem_len = usr_buf_len;

	/*
	 * If a writing operation is already in progress, we just enqueue the
	 * data and quit.
	 */
	if (sock->flags & SOCK_FLG_OP_WRITING) {
		struct wbuf_chain * wc = (struct wbuf_chain *)sock->buf;
		/*
		 * We are adding a buffer with unsent data. If we don't have any other
		 * unsent data, set the pointer to this buffer.
		 */
		if (wc->unsent == NULL) {
			wc->unsent = wbuf;
			debug_tcp_print("unsent %p remains %d\n", wbuf, wbuf->rem_len);
		}
		debug_tcp_print("returns %d\n", usr_buf_len);
		sock_reply(sock, usr_buf_len);
		/*
		 * We cannot accept new operations (write). We set the flag
		 * after sending reply not to revive only. We could deadlock.
		 */
		if (sock->buf_size >= TCP_BUF_SIZE)
			sock->flags |= SOCK_FLG_OP_PENDING;

		return;
	}

	/*
	 * Start sending data if the operation is not in progress yet. The
	 * current buffer is the nly one we have, we cannot send more.
	 */

	snd_buf_len = tcp_sndbuf((struct tcp_pcb *)sock->pcb);
	debug_tcp_print("tcp can accept %d bytes", snd_buf_len);

	wbuf->unacked = (snd_buf_len < wbuf->rem_len ? snd_buf_len : wbuf->rem_len);
	wbuf->rem_len -= wbuf->unacked;

	if (wbuf->rem_len) {
		flgs = TCP_WRITE_FLAG_MORE;
		/*
		 * Remember that this buffer has some data which we didn't pass
		 * to tcp yet.
		 */
		((struct wbuf_chain *)sock->buf)->unsent = wbuf;
		debug_tcp_print("unsent %p remains %d\n", wbuf, wbuf->rem_len);
	}

	ret = tcp_write((struct tcp_pcb *)sock->pcb, wbuf->data,
						wbuf->unacked, flgs);
	tcp_output((struct tcp_pcb *)sock->pcb);
	debug_tcp_print("%d bytes to tcp", wbuf->unacked);

	if (ret == ERR_OK) {
		/*
		 * Operation is being processed, no need to remember the message
		 * in this case, we are going to reply immediatly
		 */
		debug_tcp_print("returns %d\n", usr_buf_len);
		sock_reply(sock, usr_buf_len);
		sock->flags |= SOCK_FLG_OP_WRITING;
		if (sock->buf_size >= TCP_BUF_SIZE)
			sock->flags |= SOCK_FLG_OP_PENDING;
	} else
		sock_reply(sock, EIO);
}

static void tcp_set_conf(struct socket * sock, message * m)
{
	int err;
	nwio_tcpconf_t tconf;
	struct tcp_pcb * pcb = (struct tcp_pcb *) sock->pcb;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	assert(pcb);

	err = copy_from_user(m->m_source, &tconf, sizeof(tconf),
				(cp_grant_id_t) m->IO_GRANT, 0);

	if (err != OK)
		sock_reply(sock, err);

	debug_tcp_print("tconf.nwtc_flags = 0x%lx", tconf.nwtc_flags);
	debug_tcp_print("tconf.nwtc_remaddr = 0x%x",
				(unsigned int) tconf.nwtc_remaddr);
	debug_tcp_print("tconf.nwtc_remport = 0x%x", ntohs(tconf.nwtc_remport));
	debug_tcp_print("tconf.nwtc_locaddr = 0x%x",
				(unsigned int) tconf.nwtc_locaddr);
	debug_tcp_print("tconf.nwtc_locport = 0x%x", ntohs(tconf.nwtc_locport));

	sock->usr_flags = tconf.nwtc_flags;

	if (sock->usr_flags & NWTC_SET_RA)
		pcb->remote_ip.addr = tconf.nwtc_remaddr;
	if (sock->usr_flags & NWTC_SET_RP)
		pcb->remote_port = ntohs(tconf.nwtc_remport);

	if (sock->usr_flags & NWTC_LP_SET) {
		/* FIXME the user library can only bind to ANY anyway */
		if (tcp_bind(pcb, IP_ADDR_ANY, ntohs(tconf.nwtc_locport)) == ERR_USE) {
			sock_reply(sock, EADDRINUSE);
			return;
		}
	}
	
	sock_reply(sock, OK);
}

static void tcp_get_conf(struct socket * sock, message * m)
{
	int err;
	nwio_tcpconf_t tconf;
	struct tcp_pcb * pcb = (struct tcp_pcb *) sock->pcb;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	assert(pcb);

	tconf.nwtc_locaddr = pcb->local_ip.addr;
	tconf.nwtc_locport = htons(pcb->local_port);
	tconf.nwtc_remaddr = pcb->remote_ip.addr;
	tconf.nwtc_remport = htons(pcb->remote_port);
	tconf.nwtc_flags = sock->usr_flags;

	debug_tcp_print("tconf.nwtc_flags = 0x%lx", tconf.nwtc_flags);
	debug_tcp_print("tconf.nwtc_remaddr = 0x%x",
				(unsigned int) tconf.nwtc_remaddr);
	debug_tcp_print("tconf.nwtc_remport = 0x%x", ntohs(tconf.nwtc_remport));
	debug_tcp_print("tconf.nwtc_locaddr = 0x%x",
				(unsigned int) tconf.nwtc_locaddr);
	debug_tcp_print("tconf.nwtc_locport = 0x%x", ntohs(tconf.nwtc_locport));

	if ((unsigned int) m->COUNT < sizeof(tconf)) {
		sock_reply(sock, EINVAL);
		return;
	}
	
	err = copy_to_user(m->m_source, &tconf, sizeof(tconf),
				(cp_grant_id_t) m->IO_GRANT, 0);

	if (err != OK)
		sock_reply(sock, err);

	sock_reply(sock, OK);
}

static int enqueue_rcv_data(struct socket * sock, struct pbuf * pbuf)
{
	/* Do not enqueue more data than allowed */
	if (0 && sock->recv_data_size > 4 * TCP_BUF_SIZE)
		return ERR_MEM;

	if (sock_enqueue_data(sock, pbuf, pbuf->tot_len) != OK) {
		debug_tcp_print("data enqueueing failed");
		return ERR_MEM;
	}
	debug_tcp_print("enqueued %d bytes", pbuf->tot_len);
	//printf("enqueued %d bytes, queue %d\n", pbuf->tot_len, sock->recv_data_size);

	return ERR_OK;
}

static err_t tcp_recv_callback(void *arg,
				struct tcp_pcb *tpcb,
				struct pbuf *pbuf,
				err_t err)
{
	int ret, enqueued = 0;
	struct socket * sock = (struct socket *) arg;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	if (sock->pcb == NULL) {
		if (sock_select_set(sock))
			sock_select_notify(sock);
		return ERR_OK;
	}

	assert((struct tcp_pcb *) sock->pcb == tpcb);

	if (err != ERR_OK)
		return ERR_OK;
	if (!pbuf) {
		debug_tcp_print("tcp stream closed on the remote side");
		// sock->flags |= SOCK_FLG_CLOSED;

		/* wake up the reader and report EOF */
		if (sock->flags & SOCK_FLG_OP_PENDING &&
				sock->flags & SOCK_FLG_OP_READING) {
			sock_reply(sock, 0);
			sock->flags &= ~(SOCK_FLG_OP_PENDING |
					SOCK_FLG_OP_READING);
		}
#if 0
		/* if there are any undelivered data, drop them */
		sock_dequeue_data_all(sock, tcp_recv_free);
		tcp_abandon(tpcb, 0);
		sock->pcb = NULL;
#endif

		return ERR_OK;
	}

	/*
	 * FIXME we always enqueue the data first. If the head is empty and read
	 * operation is pending we could try to deliver immeditaly without
	 * enqueueing
	 */
	if (enqueue_rcv_data(sock, pbuf) == ERR_OK)
		enqueued = 1;

	/*
	 * Deliver data if there is a pending read operation, otherwise notify
	 * select if the socket is being monitored
	 */
	if (sock->flags & SOCK_FLG_OP_PENDING) {
		if (sock->flags & SOCK_FLG_OP_READING) {
			ret = read_from_tcp(sock, &sock->mess);
			debug_tcp_print("read op finished");
			sock_reply(sock, ret);
			sock->flags &= ~(SOCK_FLG_OP_PENDING |
					SOCK_FLG_OP_READING);
		}
	} else if (!(sock->flags & SOCK_FLG_OP_WRITING) &&
			sock_select_rw_set(sock))
		sock_select_notify(sock);

	/* perhaps we have deliverd some data to user, try to enqueue again */
	if (!enqueued) {
		return enqueue_rcv_data(sock, pbuf);
	} else
		return ERR_OK;
}

static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	struct socket * sock = (struct socket *) arg;
	struct wbuf * wbuf;
	struct wbuf_chain * wc = (struct wbuf_chain *) sock->buf;
	unsigned int snd_buf_len;
	int ret;
	
	debug_tcp_print("socket num %ld", get_sock_num(sock));

	/* an error might have had happen */
	if (sock->pcb == NULL) {
		if (sock_select_set(sock))
			sock_select_notify(sock);
		return ERR_OK;
	}

	assert((struct tcp_pcb *)sock->pcb == tpcb);

	/* operation must have been canceled, do not send any other data */
	if (!sock->flags & SOCK_FLG_OP_PENDING)
		return ERR_OK;

	wbuf = wbuf_ack_sent(sock, len);

	if (wbuf == NULL) {
		debug_tcp_print("all data acked, nothing more to send");
		sock->flags &= ~SOCK_FLG_OP_WRITING;
		if (!(sock->flags & SOCK_FLG_OP_READING))
			sock->flags &= ~SOCK_FLG_OP_PENDING;
		/* no reviving, we must notify. Write and read possible */
		if (sock_select_rw_set(sock))
			sock_select_notify(sock);
		return ERR_OK;
	}

	/* we have just freed some space, write will be accepted */
	if (sock->buf_size < TCP_BUF_SIZE && sock_select_rw_set(sock)) {
		if (!(sock->flags & SOCK_FLG_OP_READING)) {
			sock->flags &= ~SOCK_FLG_OP_PENDING;
			sock_select_notify(sock);
		}
	}

	/*
	 * Check if there is some space for new data, there should be, we just
	 * got a confirmation that some data reached the other end of the
	 * connection
	 */
	snd_buf_len = tcp_sndbuf(tpcb);
	assert(snd_buf_len > 0);
	debug_tcp_print("tcp can accept %d bytes", snd_buf_len);

	if (!wc->unsent) {
		debug_tcp_print("nothing to send");
		return ERR_OK;
	}

	wbuf = wc->unsent;
	while (wbuf) {
		unsigned int towrite;
		u8_t flgs = 0;

		towrite = (snd_buf_len < wbuf->rem_len ?
					snd_buf_len : wbuf->rem_len);
		wbuf->rem_len -= towrite;
		debug_tcp_print("data to send, sending %d", towrite);

		if (wbuf->rem_len || wbuf->next)
			flgs = TCP_WRITE_FLAG_MORE;
		ret = tcp_write(tpcb, wbuf->data + wbuf->written + wbuf->unacked,
						towrite, flgs);
		debug_tcp_print("%d bytes to tcp", towrite);

		/* tcp_output() is called once we return from this callback */

		if (ret != ERR_OK) {
			debug_print("tcp_write() failed (%d), written %d"
					, ret, wbuf->written);
			sock->flags &= ~(SOCK_FLG_OP_PENDING | SOCK_FLG_OP_WRITING);
			/* no reviving, we must notify. Write and read possible */
			if (sock_select_rw_set(sock))
				sock_select_notify(sock);
			return ERR_OK;
		}
		
		wbuf->unacked += towrite;
		snd_buf_len -= towrite;
		debug_tcp_print("tcp still accepts %d bytes\n", snd_buf_len);

		if (snd_buf_len) {
			assert(wbuf->rem_len == 0);
			wbuf = wbuf->next;
			wc->unsent = wbuf;
			if (wbuf)
				debug_tcp_print("unsent %p remains %d\n",
						wbuf, wbuf->rem_len);
			else {
				debug_tcp_print("nothing to send");
			}
		} else
			break;
	}

	return ERR_OK;
}

static err_t tcp_connected_callback(void *arg,
				struct tcp_pcb *tpcb,
				__unused err_t err)
{
	struct socket * sock = (struct socket *) arg;

	debug_tcp_print("socket num %ld err %d", get_sock_num(sock), err);

	if (sock->pcb == NULL) {
		if (sock_select_set(sock))
			sock_select_notify(sock);
		return ERR_OK;
	}

	assert((struct tcp_pcb *)sock->pcb == tpcb);

	tcp_sent(tpcb, tcp_sent_callback);
	tcp_recv(tpcb, tcp_recv_callback);
	sock_reply(sock, OK);
	sock->flags &= ~(SOCK_FLG_OP_PENDING | SOCK_FLG_OP_CONNECTING);

	/* revive does the sock_select_notify() for us */

	return ERR_OK;
}

static void tcp_op_connect(struct socket * sock)
{
	ip_addr_t remaddr;
	struct tcp_pcb * pcb;
	err_t err;

	debug_tcp_print("socket num %ld", get_sock_num(sock));
	/*
	 * Connecting is going to send some packets. Unless an immediate error
	 * occurs this operation is going to block
	 */
	sock->flags |= SOCK_FLG_OP_PENDING | SOCK_FLG_OP_CONNECTING;

	/* try to connect now */
	pcb = (struct tcp_pcb *) sock->pcb;
	remaddr = pcb->remote_ip;
	err = tcp_connect(pcb, &remaddr, pcb->remote_port,
				tcp_connected_callback);
	if (err == ERR_VAL)
		panic("Wrong tcp_connect arguments");
	if (err != ERR_OK)
		panic("Other tcp_connect error %d\n", err);
}

static int tcp_do_accept(struct socket * listen_sock,
			message * m,
			struct tcp_pcb * newpcb)
{
	struct socket * newsock;
	unsigned int sock_num;
	int ret;

	debug_tcp_print("socket num %ld", get_sock_num(listen_sock));

	if ((ret = copy_from_user(m->m_source, &sock_num, sizeof(sock_num),
				(cp_grant_id_t) m->IO_GRANT, 0)) != OK)
		return EFAULT;
	if (!is_valid_sock_num(sock_num))
		return EBADF;

	newsock = get_sock(sock_num);
	assert(newsock->pcb); /* because of previous open() */
	
	/* we really want to forget about this socket */
	tcp_err((struct tcp_pcb *)newsock->pcb, NULL);
	tcp_abandon((struct tcp_pcb *)newsock->pcb, 0);
	
	tcp_arg(newpcb, newsock);
	tcp_err(newpcb, tcp_error_callback);
	tcp_sent(newpcb, tcp_sent_callback);
	tcp_recv(newpcb, tcp_recv_callback);
	tcp_nagle_disable(newpcb);
	tcp_accepted(((struct tcp_pcb *)(listen_sock->pcb)));
	newsock->pcb = newpcb;

	debug_tcp_print("Accepted new connection using socket %d\n", sock_num);

	return OK;
}

static err_t tcp_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	struct socket * sock = (struct socket *) arg;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	assert(err == ERR_OK && newpcb);
	assert(sock->flags & SOCK_FLG_OP_LISTENING);

	if (sock->flags & SOCK_FLG_OP_PENDING) {
		int ret;

		ret = tcp_do_accept(sock, &sock->mess, newpcb);
		sock_reply(sock, ret);
		sock->flags &= ~SOCK_FLG_OP_PENDING;
		if (ret == OK) {
			return ERR_OK;
		}
		/* in case of an error fall through */
	}

	/* If we cannot accept rightaway we enqueue the connection for later */

	debug_tcp_print("Enqueue connection sock %ld pcb %p\n",
			get_sock_num(sock), newpcb);
	if (sock_enqueue_data(sock, newpcb, 1) != OK) {
		tcp_abort(newpcb);
		return ERR_ABRT;
	}
	if (sock_select_read_set(sock))
		sock_select_notify(sock);

	return ERR_OK;
}

static void tcp_op_listen(struct socket * sock, message * m)
{
	int backlog, err;
	struct tcp_pcb * new_pcb;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	err = copy_from_user(m->m_source, &backlog, sizeof(backlog),
				(cp_grant_id_t) m->IO_GRANT, 0);

	new_pcb = tcp_listen_with_backlog((struct tcp_pcb *) sock->pcb,
							(u8_t) backlog);
	debug_tcp_print("listening pcb %p", new_pcb);

	if (!new_pcb) {
		debug_tcp_print("Cannot listen on socket %ld", get_sock_num(sock));
		sock_reply(sock, EGENERIC);
		return;
	}

	/* advertise that this socket is willing to accept connections */
	tcp_accept(new_pcb, tcp_accept_callback);
	sock->flags |= SOCK_FLG_OP_LISTENING;

	sock->pcb = new_pcb;
	sock_reply(sock, OK);
}

static void tcp_op_accept(struct socket * sock, message * m)
{
	debug_tcp_print("socket num %ld", get_sock_num(sock));

	if (!(sock->flags & SOCK_FLG_OP_LISTENING)) {
		debug_tcp_print("socket %ld does not listen\n", get_sock_num(sock));
		sock_reply(sock, EINVAL);
		return;
	}

	/* there is a connection ready to be accepted */
	if (sock->recv_head) {
		int ret;
		struct tcp_pcb * pcb;
		
		pcb = (struct tcp_pcb *) sock->recv_head->data;
		assert(pcb);

		ret = tcp_do_accept(sock, m, pcb);
		sock_reply(sock, ret);
		if (ret == OK)
			sock_dequeue_data(sock);
		return;
	}

	debug_tcp_print("no ready connection, suspending\n");

	sock->flags |= SOCK_FLG_OP_PENDING;
}

static void tcp_op_shutdown_tx(struct socket * sock)
{
	err_t err;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	err = tcp_shutdown((struct tcp_pcb *) sock->pcb, 0, 1);

	switch (err) {
	case ERR_OK:
		sock_reply(sock, OK);
		break;
	case ERR_CONN:
		sock_reply(sock, ENOTCONN);
		break;
	default:
		sock_reply(sock, EGENERIC);
	}
}

static void tcp_op_get_cookie(struct socket * sock, message * m)
{
	tcp_cookie_t cookie;
	unsigned int sock_num;

	assert(sizeof(cookie) >= sizeof(sock));

	sock_num = get_sock_num(sock);
	memcpy(&cookie, &sock_num, sizeof(sock_num));

	if (copy_to_user(m->m_source, &cookie, sizeof(sock),
			(cp_grant_id_t) m->IO_GRANT, 0) == OK)
		sock_reply(sock, OK);
	else
		sock_reply(sock, EFAULT);
}

static void tcp_get_opt(struct socket * sock, message * m)
{
	int err;
	nwio_tcpopt_t tcpopt;
	struct tcp_pcb * pcb = (struct tcp_pcb *) sock->pcb;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	assert(pcb);

	if ((unsigned int) m->COUNT < sizeof(tcpopt)) {
		sock_reply(sock, EINVAL);
		return;
	}

	/* FIXME : not used by the userspace library */
	tcpopt.nwto_flags = 0;
	
	err = copy_to_user(m->m_source, &tcpopt, sizeof(tcpopt),
				(cp_grant_id_t) m->IO_GRANT, 0);

	if (err != OK)
		sock_reply(sock, err);

	sock_reply(sock, OK);
}

static void tcp_set_opt(struct socket * sock, message * m)
{
	int err;
	nwio_tcpopt_t tcpopt;
	struct tcp_pcb * pcb = (struct tcp_pcb *) sock->pcb;

	debug_tcp_print("socket num %ld", get_sock_num(sock));

	assert(pcb);

	err = copy_from_user(m->m_source, &tcpopt, sizeof(tcpopt),
				(cp_grant_id_t) m->IO_GRANT, 0);

	if (err != OK)
		sock_reply(sock, err);

	/* FIXME : The userspace library does not use this */

	sock_reply(sock, OK);
}

static void tcp_op_ioctl(struct socket * sock, message * m, __unused int blk)
{
	if (!sock->pcb) {
		sock_reply(sock, ENOTCONN);
		return;
	}

	debug_tcp_print("socket num %ld req %c %d %d",
			get_sock_num(sock),
			(m->REQUEST >> 8) & 0xff,
			m->REQUEST & 0xff,
			(m->REQUEST >> 16) & _IOCPARM_MASK);
	
	switch (m->REQUEST) {
	case NWIOGTCPCONF:
		tcp_get_conf(sock, m);
		break;
	case NWIOSTCPCONF:
		tcp_set_conf(sock, m);
		break;
	case NWIOTCPCONN:
		tcp_op_connect(sock);
		break;
	case NWIOTCPLISTENQ:
		tcp_op_listen(sock, m);
		break;
	case NWIOGTCPCOOKIE:
		tcp_op_get_cookie(sock, m);
		break;
	case NWIOTCPACCEPTTO:
		tcp_op_accept(sock, m);
		break;
	case NWIOTCPSHUTDOWN:
		tcp_op_shutdown_tx(sock);
		break;
	case NWIOGTCPOPT:
		tcp_get_opt(sock, m);
		break;
	case NWIOSTCPOPT:
		tcp_set_opt(sock, m);
		break;
	default:
		sock_reply(sock, EBADIOCTL);
		return;
	}
}

static void tcp_op_select(struct socket * sock, __unused message * m)
{
	int retsel = 0, sel;

	sel = m->USER_ENDPT;
	debug_tcp_print("socket num %ld 0x%x", get_sock_num(sock), sel);
	
	/* in this case any operation would block, no error */
	if (sock->flags & SOCK_FLG_OP_PENDING) {
		debug_tcp_print("SOCK_FLG_OP_PENDING");
		if (sel & SEL_NOTIFY) {
			if (sel & SEL_RD) {
				sock->flags |= SOCK_FLG_SEL_READ;
				debug_tcp_print("monitor read");
			}
			if (sel & SEL_WR) {
				sock->flags |= SOCK_FLG_SEL_WRITE;
				debug_tcp_print("monitor write");
			}
			if (sel & SEL_ERR)
				sock->flags |= SOCK_FLG_SEL_ERROR;
		}
		sock_reply_select(sock, 0);
		return;
	}

	if (sel & SEL_RD) {
		/*
		 * If recv_head is not NULL we can either read or accept a
		 * connection which is the same for select()
		 */
		if (sock->pcb) {
			if (sock->recv_head &&
					!(sock->flags & SOCK_FLG_OP_WRITING))
				retsel |= SEL_RD;
			else if (!(sock->flags & SOCK_FLG_OP_LISTENING) && 
					((struct tcp_pcb *) sock->pcb)->state != ESTABLISHED)
				retsel |= SEL_RD;
			else if (sel & SEL_NOTIFY) {
				sock->flags |= SOCK_FLG_SEL_READ;
				debug_tcp_print("monitor read");
			}
		} else
			retsel |= SEL_RD; /* not connected read does not block */
	}
	if (sel & SEL_WR) {
		if (sock->pcb) {
			if (((struct tcp_pcb *) sock->pcb)->state == ESTABLISHED)
				retsel |= SEL_WR;
			else if (sel & SEL_NOTIFY) {
				sock->flags |= SOCK_FLG_SEL_WRITE;
				debug_tcp_print("monitor write");
			}
		} else
			retsel |= SEL_WR; /* not connected write does not block */
	}

	if (retsel & SEL_RD) {
		debug_tcp_print("read won't block");
	}
	if (retsel & SEL_WR) {
		debug_tcp_print("write won't block");
	}

	/* we only monitor if errors will happen in the future */
	if (sel & SEL_ERR && sel & SEL_NOTIFY)
		sock->flags |= SOCK_FLG_SEL_ERROR;

	sock_reply_select(sock, retsel);
}

static void tcp_op_select_reply(struct socket * sock, message * m)
{
	assert(sock->select_ep != NONE);
	debug_tcp_print("socket num %ld", get_sock_num(sock));


	if (sock->flags & SOCK_FLG_OP_PENDING) {
		debug_tcp_print("WARNING socket still blocking!");
		return;
	}

	if (sock->flags & SOCK_FLG_SEL_READ) {
		if (sock->pcb == NULL || (sock->recv_head &&
				!(sock->flags & SOCK_FLG_OP_WRITING)) ||
			 (!(sock->flags & SOCK_FLG_OP_LISTENING) && 
			 ((struct tcp_pcb *) sock->pcb)->state !=
			 ESTABLISHED)) {
			m->DEV_SEL_OPS |= SEL_RD;
			debug_tcp_print("read won't block");
		}
	}

	if (sock->flags & SOCK_FLG_SEL_WRITE  &&
			(sock->pcb == NULL ||
			 ((struct tcp_pcb *) sock->pcb)->state ==
			 ESTABLISHED)) {
		m->DEV_SEL_OPS |= SEL_WR;
		debug_tcp_print("write won't block");
	}

	if (m->DEV_SEL_OPS) 
		sock->flags &= ~(SOCK_FLG_SEL_WRITE | SOCK_FLG_SEL_READ |
							SOCK_FLG_SEL_ERROR);
}

struct sock_ops sock_tcp_ops = {
	.open		= tcp_op_open,
	.close		= tcp_op_close,
	.read		= tcp_op_read,
	.write		= tcp_op_write,
	.ioctl		= tcp_op_ioctl,
	.select		= tcp_op_select,
	.select_reply	= tcp_op_select_reply
};

