/*
 * This file implements handling of socket-related requests from VFS
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/sysutil.h>
#include <minix/netsock.h>

#include <lwip/tcp.h>

#include <sys/ioc_net.h>

char * netsock_user_name = NULL;
#define NETSOCK_USER_NAME (netsock_user_name ? netsock_user_name : "NETSOCK")

#define debug_print(str, ...) printf("%s : %s:%d : " str "\n",	\
		NETSOCK_USER_NAME, __func__, __LINE__, ##__VA_ARGS__)

#if 0
#define debug_sock_print(...)	debug_print(__VA_ARGS__)
#else
#define debug_sock_print(...)
#endif

#if 0
#define debug_sock_select_print(...)	debug_print(__VA_ARGS__)
#else
#define debug_sock_select_print(...)	debug_sock_print(__VA_ARGS__)
#endif

#define netsock_panic(str, ...) panic("%s : " str, NETSOCK_USER_NAME, \
							##__VA_ARGS__)
#define netsock_error(str, ...) printf("%s : " str, NETSOCK_USER_NAME, \
							##__VA_ARGS__)


struct socket socket[MAX_SOCKETS];

#define recv_q_alloc()	debug_malloc(sizeof(struct recv_q))
#define recv_q_free	debug_free

struct mq {
	struct sock_req	req;
	struct mq *	prev;
	struct mq *	next;
};

#define mq_alloc()	debug_malloc(sizeof(struct mq))
#define mq_free		debug_free

static struct mq * mq_head, *mq_tail;

int mq_enqueue(struct sock_req * req)
{
	struct mq * mq;

	debug_sock_print("sock %d op %d", req->minor, req->type);
	mq = mq_alloc();

	if (mq == NULL)
		return -1;

	mq->next = NULL;
	mq->req = *req;

	if (mq_head) {
		mq->prev = mq_tail;
		mq_tail->next = mq;
		mq_tail = mq;
	}
	else {
		mq->prev = NULL;
		mq_head = mq_tail = mq;
	}

	return 0;
}

__unused static struct mq * mq_dequeue_head(void)
{
	struct mq * ret;

	if (!mq_head)
		return NULL;

	ret = mq_head;

	if (mq_head != mq_tail) {
		mq_head = mq_head->next;
		mq_head->prev = NULL;
	} else
		mq_head = mq_tail = NULL;

	debug_sock_print("socket %d\n", ret->req.minor);

	return ret;
}

static void mq_dequeue(struct mq * mq)
{
	if (mq_head == mq_tail)
		mq_head = mq_tail = NULL;
	else {
		if (mq->prev == NULL) {
			mq_head = mq->next;
			mq_head->prev = NULL;
		} else
			mq->prev->next = mq->next;
		if (mq->next == NULL) {
			mq_tail = mq->prev;
			mq_tail->next = NULL;
		} else
			mq->next->prev = mq->prev;
	}
}

static int mq_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	struct mq * mq;

	for (mq = mq_tail; mq; mq = mq->prev) {
		if (minor == mq->req.minor && endpt == mq->req.endpt &&
				id == mq->req.id) {
			debug_sock_print("socket %d\n", minor);
			break;
		}
	}

	if (mq) {
		mq_dequeue(mq);
		mq_free(mq);
	}

	/* FIXME: shouldn't this return (!!mq) ? */
	return 1;
}

int sock_enqueue_data(struct socket * sock, void * data, unsigned size)
{
	struct recv_q * r;

	if (!(r = recv_q_alloc()))
		return ENOMEM;

	r->data = data;
	r->next = NULL;

	if (sock->recv_head) {
		sock->recv_tail->next = r;
		sock->recv_tail = r;
	} else {
		sock->recv_head = sock->recv_tail = r;
	}

	assert(size > 0);
	sock->recv_data_size += size;

	return OK;
}

void * sock_dequeue_data(struct socket * sock)
{
	void * data;
	struct recv_q * r;

	if ((r = sock->recv_head)) {
		data = r->data;
		if (!(sock->recv_head = r->next))
			sock->recv_tail = NULL;
		recv_q_free(r);

		return data;
	}

	return NULL;
}

void sock_dequeue_data_all(struct socket * sock,
				recv_data_free_fn data_free)
{
	void * data;

	while ((data = sock_dequeue_data(sock)))
		data_free(data);
	sock->recv_data_size = 0;
}

static void set_reply_msg(message * m, int status)
{
	int proc, ref;

	proc= m->USER_ENDPT;
	ref= (int)m->IO_GRANT;

	m->REP_ENDPT= proc;
	m->REP_STATUS= status;
	m->REP_IO_GRANT= ref;
}

static void send_reply_type(message * m, int type, int status)
{
	int result;

	set_reply_msg(m, status);

	m->m_type = type;
	result = send(m->m_source, m);
	if (result != OK)
		netsock_panic("unable to send (err %d)", result);
}

void send_req_reply(struct sock_req * req, int status)
{
	message m;
	int result;

	if (status == EDONTREPLY)
		return;

	m.m_type = DEV_REVIVE;
	m.REP_STATUS = status;
	m.REP_ENDPT = req->endpt; /* FIXME: HACK */
	m.REP_IO_GRANT = req->id;

	result = send(req->endpt, &m);
	if (result != OK)
		netsock_panic("unable to send (err %d)", result);
}

static void send_reply(message * m, int status)
{
	debug_sock_print("status %d", status);
	send_reply_type(m, DEV_REVIVE, status);
}

static void send_reply_open(message * m, int status)
{
	debug_sock_print("status %d", status);
	send_reply_type(m, DEV_OPEN_REPL, status);
}

static void send_reply_close(message * m, int status)
{
	debug_sock_print("status %d", status);
	send_reply_type(m, DEV_CLOSE_REPL, status);
}

static void sock_reply_select(struct socket * sock, endpoint_t endpt,
	unsigned selops)
{
	int result;
	message msg;

	debug_sock_select_print("selops %d", selops);

	msg.m_type = DEV_SEL_REPL1;
	msg.DEV_MINOR = get_sock_num(sock);
	msg.DEV_SEL_OPS = selops;

	result = send(endpt, &msg);
	if (result != OK)
		netsock_panic("unable to send (err %d)", result);
}

void sock_select_notify(struct socket * sock)
{
	int result;
	message msg;

	debug_sock_select_print("socket num %ld", get_sock_num(sock));
	assert(sock->select_ep != NONE);

	msg.DEV_SEL_OPS = sock->ops->select_reply(sock);
	if (msg.DEV_SEL_OPS == 0) {
		debug_sock_select_print("called from %p sflags 0x%x TXsz %d RXsz %d\n",
				__builtin_return_address(0), sock->flags,
				sock->buf_size, sock->recv_data_size);
		return;
	}

	msg.m_type = DEV_SEL_REPL2;
	msg.DEV_MINOR = get_sock_num(sock);

	debug_sock_select_print("socket num %d select result 0x%x sent",
			msg.DEV_MINOR, msg.DEV_SEL_OPS);
	result = send(sock->select_ep, &msg);
	if (result != OK)
		netsock_panic("unable to send (err %d)", result);

	sock_clear_select(sock);
	sock->select_ep = NONE;
}

struct socket * get_unused_sock(void)
{
	int i;

	for (i = SOCK_TYPES + MAX_DEVS; i < MAX_SOCKETS; i++) {
		if (socket[i].ops == NULL) {
			/* clear it all */
			memset(&socket[i], 0, sizeof(struct socket));
			return &socket[i];
		}
	}

	return NULL;
}

static int socket_request_socket(struct socket * sock, struct sock_req * req)
{
	int r, blocking = req->flags & FLG_OP_NONBLOCK ? 0 : 1;

	switch (req->type) {
	case SOCK_REQ_READ:
		if (sock->ops && sock->ops->read)
			r = sock->ops->read(sock, req, blocking);
		else
			r = EINVAL;
		break;
	case SOCK_REQ_WRITE:
		if (sock->ops && sock->ops->write)
			r = sock->ops->write(sock, req, blocking);
		else
			r = EINVAL;
		break;
	case SOCK_REQ_IOCTL:
		if (sock->ops && sock->ops->ioctl)
			r = sock->ops->ioctl(sock, req, blocking);
		else
			r = EINVAL;
		break;
	default:
		netsock_panic("cannot happen!");
	}

	return r;
}

void socket_request(message * m)
{
	struct socket * sock;
	struct sock_req req;
	int r;

	debug_sock_print("request %d", m->m_type);
	switch (m->m_type) {
	case DEV_OPEN:
		r = socket_open(m->DEVICE);
		send_reply_open(m, r);
		return;
	case DEV_CLOSE:
		sock = get_sock(m->DEVICE);
		if (sock->ops && sock->ops->close) {
			sock->flags &= ~SOCK_FLG_OP_PENDING;
			r = sock->ops->close(sock);
		} else
			r = EINVAL;
		send_reply_close(m, r);
		return;
	case DEV_READ_S:
	case DEV_WRITE_S:
	case DEV_IOCTL_S:
		sock = get_sock(m->DEVICE);
		if (!sock) {
			send_reply(m, EINVAL);
			return;
		}
		/* Build a request record for this request. */
		req.minor = m->DEVICE;
		req.endpt = m->m_source;
		req.grant = (cp_grant_id_t) m->IO_GRANT;
		req.id = (cdev_id_t) m->IO_GRANT;
		req.flags = m->FLAGS;
		switch (m->m_type) {
		case DEV_READ_S:
		case DEV_WRITE_S:
			req.type = (m->m_type == DEV_READ_S) ?
				SOCK_REQ_READ : SOCK_REQ_WRITE;
			req.size = m->COUNT;
			break;
		case DEV_IOCTL_S:
			req.type = SOCK_REQ_IOCTL;
			req.req = m->REQUEST;
			break;
		}
		/*
		 * If an operation is pending (blocking operation) or writing is
		 * still going and we want to read, suspend the new operation
		 */
		if ((sock->flags & SOCK_FLG_OP_PENDING) ||
				(m->m_type == DEV_READ_S &&
				 sock->flags & SOCK_FLG_OP_WRITING)) {
			char * o = "\0";
			if (sock->flags & SOCK_FLG_OP_READING)
				o = "READ";
			else if (sock->flags & SOCK_FLG_OP_WRITING)
				o = "WRITE";
			else
				o = "non R/W op";
			debug_sock_print("socket %ld is busy by %s flgs 0x%x\n",
					get_sock_num(sock), o, sock->flags);
			if (mq_enqueue(&req) != 0) {
				debug_sock_print("Enqueuing suspended "
							"call failed");
				send_reply(m, ENOMEM);
			}
			return;
		}
		sock->req = req;
		r = socket_request_socket(sock, &req);
		send_req_reply(&req, r);
		return;
	case CANCEL:
		sock = get_sock(m->DEVICE);
		printf("socket num %ld\n", get_sock_num(sock));
		debug_sock_print("socket num %ld", get_sock_num(sock));
		/* Cancel the last operation in the queue */
		if (mq_cancel(m->DEVICE, m->m_source,
				(cdev_id_t) m->IO_GRANT)) {
			send_reply(m, EINTR);
		/* ... or a blocked read */
		} else if (sock->flags & SOCK_FLG_OP_PENDING &&
				sock->flags & SOCK_FLG_OP_READING) {
			sock->flags &= ~SOCK_FLG_OP_PENDING;
			send_reply(m, EINTR);
		}
		/* The request may not be found. This is OK. Do not reply. */
		return;
	case DEV_SELECT:
		/*
		 * Select is always executed immediately and is never suspended.
		 * Although, it sets actions which must be monitored
		 */
		sock = get_sock(m->DEVICE);
		assert(sock->select_ep == NONE || sock->select_ep == m->m_source);

		if (sock->ops && sock->ops->select) {
			sock->select_ep = m->m_source;
			r = sock->ops->select(sock, m->DEV_SEL_OPS);
			if (!sock_select_set(sock))
				sock->select_ep = NONE;
		} else
			r = EINVAL;

		sock_reply_select(sock, m->m_source, r);
		return;
	default:
		netsock_error("unknown message from VFS, type %d\n",
							m->m_type);
	}
	send_reply(m, EGENERIC);
}

void mq_process(void)
{
	struct mq * mq;
	struct socket * sock;
	int r;

	mq = mq_head;

	while(mq) {
		struct mq * next = mq->next;

		sock = get_sock(mq->req.minor);
		if (!(sock->flags & SOCK_FLG_OP_PENDING) &&
				!(mq->req.type == SOCK_REQ_READ &&
					sock->flags & SOCK_FLG_OP_WRITING)) {
			debug_sock_print("resuming op on sock %ld\n",
					get_sock_num(sock));
			sock->req = mq->req;
			r = socket_request_socket(sock, &sock->req);
			send_req_reply(&sock->req, r);
			mq_dequeue(mq);
			mq_free(mq);
			return;
		}

		mq = next;
	}
}

int generic_op_select(struct socket * sock, unsigned int sel)
{
	int retsel = 0;

	debug_sock_print("socket num %ld 0x%x", get_sock_num(sock), sel);

	/* in this case any operation would block, no error */
	if (sock->flags & SOCK_FLG_OP_PENDING) {
		if (sel & SEL_NOTIFY) {
			if (sel & SEL_RD)
				sock->flags |= SOCK_FLG_SEL_READ;
			if (sel & SEL_WR)
				sock->flags |= SOCK_FLG_SEL_WRITE;
			/* FIXME we do not monitor error */
		}
		return 0;
	}

	if (sel & SEL_RD) {
		if (sock->recv_head)
			retsel |= SEL_RD;
		else if (sel & SEL_NOTIFY)
			sock->flags |= SOCK_FLG_SEL_READ;
	}
	/* FIXME generic packet socket never blocks on write */
	if (sel & SEL_WR)
		retsel |= SEL_WR;
	/* FIXME SEL_ERR is ignored, we do not generate exceptions */

	return retsel;
}

int generic_op_select_reply(struct socket * sock)
{
	unsigned int sel = 0;

	assert(sock->select_ep != NONE);
	debug_sock_print("socket num %ld", get_sock_num(sock));

	/* unused for generic packet socket, see generic_op_select() */
	assert((sock->flags & (SOCK_FLG_SEL_WRITE | SOCK_FLG_SEL_ERROR)) == 0);

	if (sock->flags & SOCK_FLG_OP_PENDING) {
		debug_sock_print("WARNING socket still blocking!");
		return 0;
	}

	if (sock->flags & SOCK_FLG_SEL_READ && sock->recv_head)
		sel |= SEL_RD;

	if (sel)
		sock->flags &= ~(SOCK_FLG_SEL_WRITE | SOCK_FLG_SEL_READ |
							SOCK_FLG_SEL_ERROR);

	return sel;
}
