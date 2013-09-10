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

static int netsock_open(devminor_t minor, int access, endpoint_t user_endpt);
static int netsock_close(devminor_t minor);
static ssize_t netsock_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t netsock_write(devminor_t minor, u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id);
static int netsock_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t user_endpt, cdev_id_t id);
static int netsock_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int netsock_select(devminor_t minor, unsigned int ops,
	endpoint_t endpt);

static struct chardriver netsock_tab = {
	.cdr_open	= netsock_open,
	.cdr_close	= netsock_close,
	.cdr_read	= netsock_read,
	.cdr_write	= netsock_write,
	.cdr_ioctl	= netsock_ioctl,
	.cdr_cancel	= netsock_cancel,
	.cdr_select	= netsock_select
};

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

void send_req_reply(struct sock_req * req, int status)
{
	if (status == EDONTREPLY)
		return;

	chardriver_reply_task(req->endpt, req->id, status);
}

void sock_select_notify(struct socket * sock)
{
	unsigned int ops;

	debug_sock_select_print("socket num %ld", get_sock_num(sock));
	assert(sock->select_ep != NONE);

	ops = sock->ops->select_reply(sock);
	if (ops == 0) {
		debug_sock_select_print("called from %p sflags 0x%x TXsz %d RXsz %d\n",
				__builtin_return_address(0), sock->flags,
				sock->buf_size, sock->recv_data_size);
		return;
	}

	chardriver_reply_select(sock->select_ep, get_sock_num(sock), ops);

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
	int r, blocking = (req->flags & CDEV_NONBLOCK) ? 0 : 1;

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

static int netsock_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
	int r;

	if ((r = socket_open(minor)) < 0)
		return r;

	return CDEV_CLONED | r;
}

static int netsock_close(devminor_t minor)
{
	struct socket *sock;

	if (!(sock = get_sock(minor)))
		return EINVAL;

	if (sock->ops && sock->ops->close) {
		sock->flags &= ~SOCK_FLG_OP_PENDING;

		return sock->ops->close(sock);
	} else
		return EINVAL;
}

static int netsock_request(struct socket *sock, struct sock_req *req)
{
	char *o;

	/*
	 * If an operation is pending (blocking operation) or writing is
	 * still going on and we're reading, suspend the new operation
	 */
	if ((sock->flags & SOCK_FLG_OP_PENDING) ||
			(req->type == SOCK_REQ_READ &&
			sock->flags & SOCK_FLG_OP_WRITING)) {
		if (sock->flags & SOCK_FLG_OP_READING)
			o = "READ";
		else if (sock->flags & SOCK_FLG_OP_WRITING)
			o = "WRITE";
		else
			o = "non R/W op";
		debug_sock_print("socket %ld is busy by %s flgs 0x%x\n",
			get_sock_num(sock), o, sock->flags);

		if (mq_enqueue(req) != 0) {
			debug_sock_print("Enqueuing suspended call failed");
			return ENOMEM;
		}

		return EDONTREPLY;
	}

	return socket_request_socket(sock, req);
}

static ssize_t netsock_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
	struct socket *sock;
	struct sock_req req;

	if (!(sock = get_sock(minor)))
		return EINVAL;

	/* Build a request record for this request. */
	req.type = SOCK_REQ_READ;
	req.minor = minor;
	req.endpt = endpt;
	req.grant = grant;
	req.size = size;
	req.flags = flags;
	req.id = id;

	/* Process the request. */
	return netsock_request(sock, &req);
}

static ssize_t netsock_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
	struct socket *sock;
	struct sock_req req;

	if (!(sock = get_sock(minor)))
		return EINVAL;

	/* Build a request record for this request. */
	req.type = SOCK_REQ_WRITE;
	req.minor = minor;
	req.endpt = endpt;
	req.grant = grant;
	req.size = size;
	req.flags = flags;
	req.id = id;

	/* Process the request. */
	return netsock_request(sock, &req);
}

static int netsock_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t UNUSED(user_endpt), cdev_id_t id)
{
	struct socket *sock;
	struct sock_req req;

	if (!(sock = get_sock(minor)))
		return EINVAL;

	/* Build a request record for this request. */
	req.type = SOCK_REQ_IOCTL;
	req.minor = minor;
	req.req = request;
	req.endpt = endpt;
	req.grant = grant;
	req.flags = flags;
	req.id = id;

	/* Process the request. */
	return netsock_request(sock, &req);
}

static int netsock_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	struct socket *sock;

	if (!(sock = get_sock(minor)))
		return EDONTREPLY;

	debug_sock_print("socket num %ld", get_sock_num(sock));

	/* Cancel the last operation in the queue */
	if (mq_cancel(minor, endpt, id))
		return EINTR;

	/* Cancel any ongoing blocked read */
	if ((sock->flags & SOCK_FLG_OP_PENDING) &&
			(sock->flags & SOCK_FLG_OP_READING) &&
			endpt == sock->req.endpt && id == sock->req.id) {
		sock->flags &= ~SOCK_FLG_OP_PENDING;
		return EINTR;
	}

	/* The request may not be found. This is OK. Do not reply. */
	return EDONTREPLY;
}

static int netsock_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	struct socket *sock;
	int r;

	/*
	 * Select is always executed immediately and is never suspended.
	 * Although, it sets actions which must be monitored
	 */
	if (!(sock = get_sock(minor)))
		return EBADF;

	assert(sock->select_ep == NONE || sock->select_ep == endpt);

	if (sock->ops && sock->ops->select) {
		sock->select_ep = endpt;
		r = sock->ops->select(sock, ops);
		if (!sock_select_set(sock))
			sock->select_ep = NONE;
	} else
		r = EINVAL;

	return r;
}

void socket_request(message * m, int ipc_status)
{
	debug_sock_print("request %d", m->m_type);

	/* Let the chardriver library decode the request for us. */
	chardriver_process(&netsock_tab, m, ipc_status);
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
		if (sel & CDEV_NOTIFY) {
			if (sel & CDEV_OP_RD)
				sock->flags |= SOCK_FLG_SEL_READ;
			if (sel & CDEV_OP_WR)
				sock->flags |= SOCK_FLG_SEL_WRITE;
			/* FIXME we do not monitor error */
		}
		return 0;
	}

	if (sel & CDEV_OP_RD) {
		if (sock->recv_head)
			retsel |= CDEV_OP_RD;
		else if (sel & CDEV_NOTIFY)
			sock->flags |= SOCK_FLG_SEL_READ;
	}
	/* FIXME generic packet socket never blocks on write */
	if (sel & CDEV_OP_WR)
		retsel |= CDEV_OP_WR;
	/* FIXME CDEV_OP_ERR is ignored, we do not generate exceptions */

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
		sel |= CDEV_OP_RD;

	if (sel)
		sock->flags &= ~(SOCK_FLG_SEL_WRITE | SOCK_FLG_SEL_READ |
							SOCK_FLG_SEL_ERROR);

	return sel;
}
