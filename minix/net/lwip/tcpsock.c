/* LWIP service - tcpsock.c - TCP sockets */
/*
 * This module implements support for TCP sockets based on lwIP's core TCP PCB
 * module, which is largely but not fully cooperative with exactly what we want
 * to achieve, with as a result that this module is rather complicated.
 *
 * Each socket has a send queue and a receive queue.  Both are using lwIP's own
 * (pbuf) buffers, which largely come out of the main 512-byte buffer pool.
 * The buffers on the send queue are allocated and freed by us--the latter only
 * once they are no longer in use by lwIP as well.  A bit counterintuitively,
 * we deliberately use a smaller lwIP per-PCB TCP send buffer limit
 * (TCP_SND_BUF) in the lwIP send configuration (lwipopts.h) in order to more
 * easily trigger conditions where we cannot enqueue data (or the final FIN)
 * right away.  This way, we get to test the internal logic of this module a
 * lot more easily.  The small lwIP send queue size should not have any impact
 * on performance, as our own per-socket send queues can be much larger and we
 * enqueue more of that on the lwIP PCB as soon as we can in all cases.
 *
 * The receive queue consists of whatever buffers were given to us by lwIP, but
 * since those may be many buffers with small amounts of data each, we perform
 * fairly aggressive merging of consecutive buffers.  The intended result is
 * that we waste no more than 50% of memory within the receive queue.  Merging
 * requires memory copies, which makes it expensive, but we do not configure
 * lwIP with enough buffers to make running out of buffers a non-issue, so this
 * trade-off is necessary.  Practical experience and measurements of the merge
 * policy will have to show whether and how the current policy may be improved.
 *
 * As can be expected, the connection close semantics are by far the most
 * complicated part of this module.  We attempt to get rid of the lwIP PCB as
 * soon as we can, letting lwIP take care of the TIME_WAIT state for example.
 * However, there are various conditions that have to be met before we can
 * forget about the PCB here--most importantly, that none of our sent data
 * blocks are still referenced by lwIP because they have not yet been sent or
 * acknowledged.  We can only free the data blocks once lwIP is done with them.
 *
 * We do consider the TCP state of lwIP's PCB, in order to avoid duplicating
 * full state tracking here.  However, we do not look at a socket's TCP state
 * while in a lwIP-generated event for that socket, because the state may not
 * necessarily reflect the (correct or new) TCP state of the connection, nor
 * may the PCB be available--this is the case for error events.  For these
 * reasons we use a few internal TCPF_ flags to perform partial state tracking.
 *
 * More generally, we tend to access lwIP PCB fields directly only when lwIP's
 * own BSD API implementation does that too and there is no better alternative.
 * One example of this is the check to see if our FIN was acknowledged, for
 * SO_LINGER support.  In terms of maintenance, our hope is that if lwIP's API
 * changes later, we can change our code to imitate whatever lwIP's BSD API
 * implementation does at that point.
 */

#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>

/*
 * Unfortunately, NetBSD and lwIP have different definitions of a few relevant
 * preprocessor variables.  Make sure we do not attempt to use the NetBSD one
 * where it matters.  We do need one of the NetBSD definitions though.
 */
static const unsigned int NETBSD_TF_NODELAY = TF_NODELAY;
#undef TF_NODELAY
#undef TCP_MSS

#include "lwip.h"
#include "tcpisn.h"

#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h" /* for tcp_pcb_lists */

/*
 * The number of TCP sockets (NR_TCPSOCK) is defined in the lwIP configuration.
 */

/*
 * We fully control the send buffer, so we can let its size be set to whatever
 * we want.  The receive buffer is different: if it is smaller than the window
 * size, we may have to refuse data that lwIP hands us, at which point more
 * incoming data will cause lwIP to abort the TCP connection--even aside from
 * performance issues.  Therefore, we must make sure the receive buffer is
 * larger than the TCP window at all times.
 */
#define TCP_SNDBUF_MIN	1		/* minimum TCP send buffer size */
#define TCP_SNDBUF_DEF	32768		/* default TCP send buffer size */
#define TCP_SNDBUF_MAX	131072		/* maximum TCP send buffer size */
#define TCP_RCVBUF_MIN	TCP_WND		/* minimum TCP receive buffer size */
#define TCP_RCVBUF_DEF	MAX(TCP_WND, 32768) /* default TCP recv buffer size */
#define TCP_RCVBUF_MAX	MAX(TCP_WND, 131072) /* maximum TCP recv buffer size */

/*
 * The total number of buffers that may in use for TCP socket send queues.  The
 * goal is to allow at least some progress to be made on receiving from TCP
 * sockets and on differently-typed sockets, at least as long as the LWIP
 * service can manage to allocate the memory it wants.  For the case that it
 * does not, we can only reactively kill off TCP sockets and/or free enqueued
 * ethernet packets, neither of which is currently implemented (TODO).
 */
#define TCP_MAX_SENDBUFS	(mempool_max_buffers() * 3 / 4)

/* Polling intervals, in 500-millsecond units. */
#define TCP_POLL_REG_INTERVAL	10	/* interval for reattempting sends */
#define TCP_POLL_CLOSE_INTERVAL	1	/* interval while closing connection */

static struct tcpsock {
	struct ipsock tcp_ipsock;		/* IP socket, MUST be first */
	struct tcp_pcb *tcp_pcb;		/* lwIP TCP control block */
	union pxfer_tcp_queue {			/* free/accept queue */
		TAILQ_ENTRY(tcpsock) tq_next;	/* next in queue */
		TAILQ_HEAD(, tcpsock) tq_head;	/* head of queue */
	} tcp_queue;
	struct tcpsock *tcp_listener;		/* listener if on accept q. */
	struct {				/* send queue */
		struct pbuf *ts_head;		/* first pbuf w/unacked data */
		struct pbuf *ts_unsent;		/* first pbuf w/unsent data */
		struct pbuf *ts_tail;		/* most recently added data */
		size_t ts_len;			/* total sent + unsent */
		unsigned short ts_head_off;	/* offset into head pbuf */
		unsigned short ts_unsent_off;	/* offset into unsent pbuf */
	} tcp_snd;
	struct {				/* receive queue */
		struct pbuf *tr_head;		/* first pbuf w/unrecvd data */
		struct pbuf **tr_pre_tailp;	/* ptr-ptr to newest pbuf */
		size_t tr_len;			/* bytes on receive queue */
		unsigned short tr_head_off;	/* offset into head pbuf */
		unsigned short tr_unacked;	/* current window reduction */
	} tcp_rcv;
} tcp_array[NR_TCPSOCK];

static TAILQ_HEAD(, tcpsock) tcp_freelist;	/* list of free TCP sockets */

static const struct sockevent_ops tcpsock_ops;

static unsigned int tcpsock_sendbufs;		/* # send buffers in use */
static unsigned int tcpsock_recvbufs;		/* # receive buffers in use */

/* A bunch of macros that are just for convenience. */
#define tcpsock_get_id(tcp)	(SOCKID_TCP | (sockid_t)((tcp) - tcp_array))
#define tcpsock_get_ipsock(tcp)	(&(tcp)->tcp_ipsock)
#define tcpsock_get_sock(tcp)	(ipsock_get_sock(tcpsock_get_ipsock(tcp)))
#define tcpsock_get_sndbuf(tcp)	(ipsock_get_sndbuf(tcpsock_get_ipsock(tcp)))
#define tcpsock_get_rcvbuf(tcp)	(ipsock_get_rcvbuf(tcpsock_get_ipsock(tcp)))
#define tcpsock_is_ipv6(tcp)	(ipsock_is_ipv6(tcpsock_get_ipsock(tcp)))
#define tcpsock_is_shutdown(tcp,fl) \
	(sockevent_is_shutdown(tcpsock_get_sock(tcp), fl))
#define tcpsock_is_listening(tcp) \
	(sockevent_is_listening(tcpsock_get_sock(tcp)))
#define tcpsock_get_flags(tcp)	(ipsock_get_flags(tcpsock_get_ipsock(tcp)))
#define tcpsock_set_flag(tcp,fl) \
	(ipsock_set_flag(tcpsock_get_ipsock(tcp), fl))
#define tcpsock_clear_flag(tcp,fl) \
	(ipsock_clear_flag(tcpsock_get_ipsock(tcp), fl))

static ssize_t tcpsock_pcblist(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

/* The CTL_NET {PF_INET,PF_INET6} IPPROTO_TCP subtree. */
/* TODO: add many more and make some of them writable.. */
static struct rmib_node net_inet_tcp_table[] = {
/* 2*/	[TCPCTL_SENDSPACE]	= RMIB_INT(RMIB_RO, TCP_SNDBUF_DEF,
				    "sendspace",
				    "Default TCP send buffer size"),
/* 3*/	[TCPCTL_RECVSPACE]	= RMIB_INT(RMIB_RO, TCP_RCVBUF_DEF,
				    "recvspace",
				    "Default TCP receive buffer size"),
/*29*/	[TCPCTL_LOOPBACKCKSUM]	= RMIB_FUNC(RMIB_RW | CTLTYPE_INT, sizeof(int),
				    loopif_cksum, "do_loopback_cksum",
				    "Perform TCP checksum on loopback"),
/*+0*/	[TCPCTL_MAXID]		= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0,
				    tcpsock_pcblist, "pcblist",
				    "TCP protocol control block list"),
/*+1*/	[TCPCTL_MAXID + 1]	= RMIB_FUNC(RMIB_RW | CTLFLAG_PRIVATE |
				    CTLFLAG_HIDDEN | CTLTYPE_STRING,
				    TCPISN_SECRET_HEX_LENGTH, tcpisn_secret,
				    "isn_secret",
				    "TCP ISN secret (MINIX 3 specific)")
};

static struct rmib_node net_inet_tcp_node =
    RMIB_NODE(RMIB_RO, net_inet_tcp_table, "tcp", "TCP related settings");
static struct rmib_node net_inet6_tcp6_node =
    RMIB_NODE(RMIB_RO, net_inet_tcp_table, "tcp6", "TCP related settings");

/*
 * Initialize the TCP sockets module.
 */
void
tcpsock_init(void)
{
	unsigned int slot;

	/* Initialize the list of free TCP sockets. */
	TAILQ_INIT(&tcp_freelist);

	for (slot = 0; slot < __arraycount(tcp_array); slot++)
		TAILQ_INSERT_TAIL(&tcp_freelist, &tcp_array[slot],
		    tcp_queue.tq_next);

	/* Initialize other variables. */
	tcpsock_sendbufs = 0;

	/* Register the net.inet.tcp and net.inet6.tcp6 RMIB subtrees. */
	mibtree_register_inet(PF_INET, IPPROTO_TCP, &net_inet_tcp_node);
	mibtree_register_inet(PF_INET6, IPPROTO_TCP, &net_inet6_tcp6_node);
}

/*
 * Initialize the state of a TCP socket's send queue.
 */
static void
tcpsock_reset_send(struct tcpsock * tcp)
{

	tcp->tcp_snd.ts_tail = NULL;
	tcp->tcp_snd.ts_unsent = NULL;
	tcp->tcp_snd.ts_head = NULL;
	tcp->tcp_snd.ts_len = 0;
	tcp->tcp_snd.ts_unsent_off = 0;
	tcp->tcp_snd.ts_head_off = 0;
}

/*
 * Initialize the state of a TCP socket's receive queue.
 */
static void
tcpsock_reset_recv(struct tcpsock * tcp)
{

	tcp->tcp_rcv.tr_pre_tailp = NULL;
	tcp->tcp_rcv.tr_head = NULL;
	tcp->tcp_rcv.tr_len = 0;
	tcp->tcp_rcv.tr_head_off = 0;
	tcp->tcp_rcv.tr_unacked = 0;
}

/*
 * Create a TCP socket.
 */
sockid_t
tcpsock_socket(int domain, int protocol, struct sock ** sockp,
	const struct sockevent_ops ** ops)
{
	struct tcpsock *tcp;
	uint8_t ip_type;

	switch (protocol) {
	case 0:
	case IPPROTO_TCP:
		break;

	default:
		return EPROTONOSUPPORT;
	}

	if (TAILQ_EMPTY(&tcp_freelist))
		return ENOBUFS;

	tcp = TAILQ_FIRST(&tcp_freelist);

	/*
	 * Initialize the structure.  Do not memset it to zero, as it is still
	 * part of the linked free list.  Initialization may still fail.  When
	 * adding new fields, make sure to change tcpsock_clone() accordingly.
	 */

	ip_type = ipsock_socket(tcpsock_get_ipsock(tcp), domain,
	    TCP_SNDBUF_DEF, TCP_RCVBUF_DEF, sockp);

	if ((tcp->tcp_pcb = tcp_new_ip_type(ip_type)) == NULL)
		return ENOBUFS;
	tcp_arg(tcp->tcp_pcb, tcp);

	tcp->tcp_listener = NULL;

	tcpsock_reset_send(tcp);
	tcpsock_reset_recv(tcp);

	TAILQ_REMOVE(&tcp_freelist, tcp, tcp_queue.tq_next);

	*ops = &tcpsock_ops;
	return tcpsock_get_id(tcp);
}

/*
 * Create a TCP socket for the TCP PCB 'pcb' which identifies a new connection
 * incoming on listening socket 'listener'.  The new socket is essentially a
 * "clone" of the listening TCP socket, in that it should inherit any settings
 * from the listening socket.  The socket has not yet been accepted by userland
 * so add it to the queue of connetions pending for the listening socket.  On
 * success, return OK.  On failure, return a negative error code.
 */
static int
tcpsock_clone(struct tcpsock * listener, struct tcp_pcb * pcb)
{
	struct tcpsock *tcp;

	if (TAILQ_EMPTY(&tcp_freelist))
		return ENOBUFS;

	tcp = TAILQ_FIRST(&tcp_freelist);

	/*
	 * Initialize the structure.  Do not memset it to zero, as it is still
	 * part of the linked free list.  Initialization may still fail.  Most
	 * settings should be inherited from the listening socket here, rather
	 * than being initialized to their default state.
	 */

	ipsock_clone(tcpsock_get_ipsock(listener), tcpsock_get_ipsock(tcp),
	    tcpsock_get_id(tcp));

	tcp->tcp_pcb = pcb;
	tcp_arg(pcb, tcp);

	tcpsock_reset_send(tcp);
	tcpsock_reset_recv(tcp);

	/*
	 * Remove the new socket from the free list, and add it to the queue of
	 * the listening socket--in this order, because the same next pointer
	 * is used for both.
	 */
	TAILQ_REMOVE(&tcp_freelist, tcp, tcp_queue.tq_next);

	TAILQ_INSERT_TAIL(&listener->tcp_queue.tq_head, tcp,
	    tcp_queue.tq_next);
	tcp->tcp_listener = listener;

	return OK;
}

/*
 * Allocate a buffer from the pool, using the standard pool size.  The returned
 * buffer is a single element--never a chain.
 */
static struct pbuf *
tcpsock_alloc_buf(void)
{
	struct pbuf *pbuf;

	pbuf = pbuf_alloc(PBUF_RAW, MEMPOOL_BUFSIZE, PBUF_RAM);

	assert(pbuf == NULL || pbuf->len == pbuf->tot_len);

	return pbuf;
}

/*
 * Free the given buffer.  Ensure that pbuf_free() will not attempt to free the
 * next buffer(s) in the chain as well.  This may be called for pbufs other
 * than those allocated with tcpsock_alloc_buf().
 */
static void
tcpsock_free_buf(struct pbuf * pbuf)
{

	/*
	 * Resetting the length is currently not necessary, but better safe
	 * than sorry..
	 */
	pbuf->len = pbuf->tot_len;
	pbuf->next = NULL;

	pbuf_free(pbuf);
}

/*
 * Clear the send queue of a TCP socket.  The caller must ensure that lwIP will
 * no longer access any of data on the send queue.
 */
static void
tcpsock_clear_send(struct tcpsock * tcp)
{
	struct pbuf *phead;

	assert(tcp->tcp_pcb == NULL);

	while ((phead = tcp->tcp_snd.ts_head) != NULL) {
		tcp->tcp_snd.ts_head = phead->next;

		assert(tcpsock_sendbufs > 0);
		tcpsock_sendbufs--;

		tcpsock_free_buf(phead);
	}

	tcpsock_reset_send(tcp);
}

/*
 * Clear the receive queue of a TCP socket.  If 'ack_data' is set, also
 * acknowledge the previous contents of the receive queue to lwIP.
 */
static size_t
tcpsock_clear_recv(struct tcpsock * tcp, int ack_data)
{
	struct pbuf *phead;
	size_t rlen;

	rlen = tcp->tcp_rcv.tr_len;

	while ((phead = tcp->tcp_rcv.tr_head) != NULL) {
		tcp->tcp_rcv.tr_head = phead->next;

		assert(tcpsock_recvbufs > 0);
		tcpsock_recvbufs--;

		tcpsock_free_buf(phead);
	}

	/*
	 * From now on, we will basically be discarding incoming data as fast
	 * as possible, to keep the full window open at all times.
	 */
	if (ack_data && tcp->tcp_pcb != NULL && tcp->tcp_rcv.tr_unacked > 0)
		tcp_recved(tcp->tcp_pcb, tcp->tcp_rcv.tr_unacked);

	tcpsock_reset_recv(tcp);

	return rlen;
}

/*
 * The TCP socket's PCB has been detached from the socket, typically because
 * the connection was aborted, either by us or by lwIP.  Either way, any TCP
 * connection is gone.  Clear the socket's send queue, remove the socket from
 * a listening socket's queue, and if the socket itself is ready and allowed to
 * be freed, free it now.  The socket is ready to be freed if it was either on
 * a listening queue or being closed already.  The socket is allowed to be
 * freed only if 'may_free' is TRUE.  If the socket is not freed, its receive
 * queue is left as is, as it may still have data to be received by userland.
 */
static int
tcpsock_cleanup(struct tcpsock * tcp, int may_free)
{
	int destroy;

	assert(tcp->tcp_pcb == NULL);

	/*
	 * Free any data on the send queue.  This is safe to do right now,
	 * because the PCB has been aborted (or was already gone).  We must be
	 * very careful about clearing the send queue in all other situations.
	 */
	tcpsock_clear_send(tcp);

	/*
	 * If this was a socket pending acceptance, remove it from the
	 * corresponding listener socket's queue, and free it.  Otherwise, free
	 * the socket only if it suspended a graceful close operation.
	 */
	if (tcp->tcp_listener != NULL) {
		TAILQ_REMOVE(&tcp->tcp_listener->tcp_queue.tq_head, tcp,
		    tcp_queue.tq_next);
		tcp->tcp_listener = NULL;

		/*
		 * The listener socket's backlog count should be adjusted by
		 * lwIP whenever the PCB is freed up, so we need (and must) not
		 * attempt to do that here.
		 */

		destroy = TRUE;
	} else
		destroy = sockevent_is_closing(tcpsock_get_sock(tcp));

	/*
	 * Do not free the socket if 'may_free' is FALSE.  That flag may be set
	 * if we are currently in the second tcpsock_close() call on the
	 * socket, in which case sockevent_is_closing() is TRUE but we must
	 * still not free the socket now: doing so would derail libsockevent.
	 */
	if (destroy && may_free) {
		(void)tcpsock_clear_recv(tcp, FALSE /*ack_data*/);

		sockevent_raise(tcpsock_get_sock(tcp), SEV_CLOSE);
	}

	return destroy;
}

/*
 * Abort the lwIP PCB for the given socket, using tcp_abort().  If the PCB is
 * connected, this will cause the connection to be reset.  The PCB, which must
 * have still been present before the call, will be gone after the call.
 */
static void
tcpsock_pcb_abort(struct tcpsock * tcp)
{

	assert(tcp->tcp_pcb != NULL);
	assert(!tcpsock_is_listening(tcp));

	tcp_recv(tcp->tcp_pcb, NULL);
	tcp_sent(tcp->tcp_pcb, NULL);
	tcp_err(tcp->tcp_pcb, NULL);
	tcp_poll(tcp->tcp_pcb, NULL, TCP_POLL_REG_INTERVAL);

	tcp_arg(tcp->tcp_pcb, NULL);

	tcp_abort(tcp->tcp_pcb);

	tcp->tcp_pcb = NULL;
}

/*
 * Close the lwIP PCB for the given socket, using tcp_close().  If the PCB is
 * connected, its graceful close will be finished by lwIP in the background.
 * The PCB, which must have still been present before the call, will be gone
 * after the call.
 */
static void
tcpsock_pcb_close(struct tcpsock * tcp)
{
	err_t err;

	assert(tcp->tcp_pcb != NULL);
	assert(tcp->tcp_snd.ts_len == 0);

	if (!tcpsock_is_listening(tcp)) {
		tcp_recv(tcp->tcp_pcb, NULL);
		tcp_sent(tcp->tcp_pcb, NULL);
		tcp_err(tcp->tcp_pcb, NULL);
		tcp_poll(tcp->tcp_pcb, NULL, TCP_POLL_REG_INTERVAL);
	}

	tcp_arg(tcp->tcp_pcb, NULL);

	if ((err = tcp_close(tcp->tcp_pcb)) != ERR_OK)
		panic("unexpected TCP close failure: %d", err);

	tcp->tcp_pcb = NULL;
}

/*
 * Return TRUE if all conditions are met for closing the TCP socket's PCB, or
 * FALSE if they are not.  Upon calling this function, the socket's PCB must
 * still be around.
 */
static int
tcpsock_may_close(struct tcpsock * tcp)
{

	assert(tcp->tcp_pcb != NULL);

	/*
	 * Regular closing of the PCB requires three conditions to be met:
	 *
	 * 1. all our data has been transmitted AND acknowledged, so that we do
	 *    not risk corruption in case there are still unsent or unack'ed
	 *    data buffers that may otherwise be recycled too soon;
	 * 2. we have sent our FIN to the peer; and,
	 * 3. we have received a FIN from the peer.
	 */
	return ((tcpsock_get_flags(tcp) & (TCPF_SENT_FIN | TCPF_RCVD_FIN)) ==
	    (TCPF_SENT_FIN | TCPF_RCVD_FIN) && tcp->tcp_snd.ts_len == 0);
}

/*
 * The given socket is ready to be closed as per the tcpsock_may_close() rules.
 * This implies that its send queue is already empty.  Gracefully close the
 * PCB.  In addition, if the socket is being closed gracefully, meaning we
 * suspended an earlier tcpsock_close() call (and as such already emptied the
 * receive queue as well), then tell libsockevent that the close is finished,
 * freeing the socket.  Return TRUE if the socket has indeed been freed this
 * way, or FALSE if the socket is still around.
 */
static int
tcpsock_finish_close(struct tcpsock * tcp)
{

	assert(tcp->tcp_snd.ts_len == 0);
	assert(tcp->tcp_listener == NULL);

	/*
	 * If we get here, we have already shut down the sending side of the
	 * PCB.  Technically, we are interested only in shutting down the
	 * receiving side of the PCB here, so that lwIP may decide to recycle
	 * the socket later etcetera.  We call tcp_close() because we do not
	 * want to rely on tcp_shutdown(RX) doing the exact same thing.
	 * However, we do rely on the fact that the PCB is not immediately
	 * destroyed by the tcp_close() call: otherwise we may have to return
	 * ERR_ABRT if this function is called from a lwIP-generated event.
	 */
	tcpsock_pcb_close(tcp);

	/*
	 * If we suspended an earlier tcpsock_close() call, we have to tell
	 * libsockevent that the close operation is now complete.
	 */
	if (sockevent_is_closing(tcpsock_get_sock(tcp))) {
		assert(tcp->tcp_rcv.tr_len == 0);

		sockevent_raise(tcpsock_get_sock(tcp), SEV_CLOSE);

		return TRUE;
	} else
		return FALSE;
}

/*
 * Attempt to start or resume enqueuing data and/or a FIN to send on the given
 * TCP socket.  Return TRUE if anything at all could be newly enqueued on the
 * lwIP PCB, even if less than desired.  In that case, the caller should try to
 * send whatever was enqueued, and if applicable, check if the socket may now
 * be closed (due to the FIN being enqueued).  In particular, in any situation
 * where the socket may be in the process of being closed, the caller must use
 * tcpsock_may_close() if TRUE is returned.  Return FALSE if nothing new could
 * be enqueued, in which case no send attempt need to be made either.
 */
static int
tcpsock_pcb_enqueue(struct tcpsock * tcp)
{
	struct pbuf *punsent;
	size_t space, chunk;
	unsigned int flags;
	err_t err;
	int enqueued;

	assert(tcp->tcp_pcb != NULL);

	if (tcpsock_get_flags(tcp) & TCPF_FULL)
		return FALSE;

	/*
	 * Attempt to enqueue more unsent data, if any, on the PCB's send
	 * queue.
	 */
	enqueued = FALSE;

	while (tcp->tcp_snd.ts_unsent != NULL) {
		if ((space = tcp_sndbuf(tcp->tcp_pcb)) == 0)
			break;

		/*
		 * We may maintain a non-NULL unsent pointer even when there is
		 * nothing more to send right now, because the tail buffer may
		 * be filled up further later on.
		 */
		punsent = tcp->tcp_snd.ts_unsent;

		assert(punsent->len >= tcp->tcp_snd.ts_unsent_off);

		chunk = (size_t)punsent->len - tcp->tcp_snd.ts_unsent_off;
		if (chunk == 0)
			break;

		if (chunk > space)
			chunk = space;

		/* Try to enqueue more data for sending. */
		if (chunk < punsent->len || punsent->next != NULL)
			flags = TCP_WRITE_FLAG_MORE;
		else
			flags = 0;

		err = tcp_write(tcp->tcp_pcb, (char *)punsent->payload +
		    tcp->tcp_snd.ts_unsent_off, chunk, flags);

		/*
		 * Since tcp_write() enqueues data only, it should only return
		 * out-of-memory errors; no fatal ones.  In any case, stop.
		 */
		if (err != ERR_OK) {
			assert(err == ERR_MEM);

			break;
		}

		/* We have successfully enqueued data. */
		enqueued = TRUE;

		tcp->tcp_snd.ts_unsent_off += chunk;

		if (tcp->tcp_snd.ts_unsent_off < punsent->tot_len) {
			assert(tcp->tcp_snd.ts_unsent_off < punsent->len ||
			    punsent->next == NULL);

			break;
		}

		tcp->tcp_snd.ts_unsent = punsent->next;
		tcp->tcp_snd.ts_unsent_off = 0;
	}

	/*
	 * If all pending data has been enqueued for sending, and we should
	 * shut down the sending end of the socket, try that now.
	 */
	if ((tcp->tcp_snd.ts_unsent == NULL ||
	    tcp->tcp_snd.ts_unsent_off == tcp->tcp_snd.ts_unsent->len) &&
	    tcpsock_is_shutdown(tcp, SFL_SHUT_WR) &&
	    !(tcpsock_get_flags(tcp) & TCPF_SENT_FIN)) {
		err = tcp_shutdown(tcp->tcp_pcb, 0 /*shut_rx*/, 1 /*shut_tx*/);

		if (err == ERR_OK) {
			/*
			 * We have successfully enqueued a FIN.  The caller is
			 * now responsible for checking whether the PCB and
			 * possibly even the socket object can now be freed.
			 */
			tcpsock_set_flag(tcp, TCPF_SENT_FIN);

			enqueued = TRUE;
		} else {
			assert(err == ERR_MEM);

			/*
			 * FIXME: the resolution for lwIP bug #47485 has taken
			 * away even more control over the closing process from
			 * us, making tracking sockets especially for SO_LINGER
			 * even harder.  For now, we simply effectively undo
			 * the patch by clearing TF_CLOSEPEND if tcp_shutdown()
			 * returns ERR_MEM.  This will not be sustainable in
			 * the long term, though.
			 */
			tcp->tcp_pcb->flags &= ~TF_CLOSEPEND;

			tcpsock_set_flag(tcp, TCPF_FULL);
		}
	}

	return enqueued;
}

/*
 * Request lwIP to start sending any enqueued data and/or FIN on the TCP
 * socket's lwIP PCB.  On success, return OK.  On failure, return a negative
 * error code, after cleaning up the socket, freeing the PCB.  If the socket
 * was already being closed, also free the socket object in that case; the
 * caller must then not touch the socket object anymore upon return.  If the
 * socket object is not freed, and if 'raise_error' is TRUE, raise the error
 * on the socket object.
 */
static int
tcpsock_pcb_send(struct tcpsock * tcp, int raise_error)
{
	err_t err;
	int r;

	assert(tcp->tcp_pcb != NULL);

	/*
	 * If we have enqueued something, ask lwIP to send TCP packets now.
	 * This may result in a fatal error, in which case we clean up the
	 * socket and return the error to the caller.  Since cleaning up the
	 * socket may free the socket object, and the caller cannot tell
	 * whether that will happen or has happened, also possibly raise the
	 * error on the socket object if it is not gone.  As such, callers that
	 * set 'raise_error' to FALSE must know for sure that the socket was
	 * not being closed, for example because the caller is processing a
	 * (send) call from userland.
	 */
	err = tcp_output(tcp->tcp_pcb);

	if (err != ERR_OK && err != ERR_MEM) {
		tcpsock_pcb_abort(tcp);

		r = util_convert_err(err);

		if (!tcpsock_cleanup(tcp, TRUE /*may_free*/)) {
			if (raise_error)
				sockevent_set_error(tcpsock_get_sock(tcp), r);
		}
		/* Otherwise, do not touch the socket object anymore! */

		return r;
	} else
		return OK;
}

/*
 * Callback from lwIP.  The given number of data bytes have been acknowledged
 * as received by the remote end.  Dequeue and free data from the TCP socket's
 * send queue as appropriate.
 */
static err_t
tcpsock_event_sent(void * arg, struct tcp_pcb * pcb __unused, uint16_t len)
{
	struct tcpsock *tcp = (struct tcpsock *)arg;
	struct pbuf *phead;
	size_t left;

	assert(tcp != NULL);
	assert(pcb == tcp->tcp_pcb);
	assert(len > 0);

	assert(tcp->tcp_snd.ts_len >= len);
	assert(tcp->tcp_snd.ts_head != NULL);

	left = len;

	/*
	 * First see if we can free up whole buffers.  Check against the head
	 * buffer's 'len' rather than 'tot_len', or we may end up leaving an
	 * empty buffer on the chain.
	 */
	while ((phead = tcp->tcp_snd.ts_head) != NULL &&
	    left >= (size_t)phead->len - tcp->tcp_snd.ts_head_off) {
		left -= (size_t)phead->len - tcp->tcp_snd.ts_head_off;

		tcp->tcp_snd.ts_head = phead->next;
		tcp->tcp_snd.ts_head_off = 0;

		if (phead == tcp->tcp_snd.ts_unsent) {
			assert(tcp->tcp_snd.ts_unsent_off == phead->len);

			tcp->tcp_snd.ts_unsent = phead->next;
			tcp->tcp_snd.ts_unsent_off = 0;
		}

		assert(tcpsock_sendbufs > 0);
		tcpsock_sendbufs--;

		tcpsock_free_buf(phead);
	}

	/*
	 * The rest of the given length is for less than the current head
	 * buffer.
	 */
	if (left > 0) {
		assert(tcp->tcp_snd.ts_head != NULL);
		assert((size_t)tcp->tcp_snd.ts_head->len -
		    tcp->tcp_snd.ts_head_off > left);

		tcp->tcp_snd.ts_head_off += left;
	}

	tcp->tcp_snd.ts_len -= (size_t)len;

	if (tcp->tcp_snd.ts_head == NULL) {
		assert(tcp->tcp_snd.ts_len == 0);
		assert(tcp->tcp_snd.ts_unsent == NULL);
		tcp->tcp_snd.ts_tail = NULL;
	} else
		assert(tcp->tcp_snd.ts_len > 0);

	/*
	 * If we emptied the send queue, and we already managed to send a FIN
	 * earlier, we may now have met all requirements to close the socket's
	 * PCB.  Otherwise, we may also be able to send more now, so try to
	 * resume sending.  Since we are invoked from the "sent" event,
	 * tcp_output() will not actually process anything, and so we do not
	 * call it either.  If we did, we would have to deal with errors here.
	 */
	if (tcpsock_may_close(tcp)) {
		if (tcpsock_finish_close(tcp))
			return ERR_OK;
	} else {
		tcpsock_clear_flag(tcp, TCPF_FULL);

		/*
		 * If we now manage to enqueue a FIN, we may be ready to close
		 * the PCB after all.
		 */
		if (tcpsock_pcb_enqueue(tcp)) {
			if (tcpsock_may_close(tcp) &&
			    tcpsock_finish_close(tcp))
				return ERR_OK;
		}
	}

	/* The user may also be able to send more now. */
	sockevent_raise(tcpsock_get_sock(tcp), SEV_SEND);

	return ERR_OK;
}

/*
 * Check whether any (additional) data previously received on a TCP socket
 * should be acknowledged, possibly allowing the remote end to send additional
 * data as a result.
 */
static void
tcpsock_ack_recv(struct tcpsock * tcp)
{
	size_t rcvbuf, left, delta, ack;

	assert(tcp->tcp_pcb != NULL);

	/*
	 * We must make sure that at all times, we can still add an entire
	 * window's worth of data to the receive queue.  If the amount of free
	 * space drops below that threshold, we stop acknowledging received
	 * data.  The user may change the receive buffer size at all times; we
	 * update the window size lazily as appropriate.
	 */
	rcvbuf = tcpsock_get_rcvbuf(tcp);

	if (rcvbuf > tcp->tcp_rcv.tr_len && tcp->tcp_rcv.tr_unacked > 0) {
		/*
		 * The number of bytes that lwIP can still give us at any time
		 * is represented as 'left'.  The number of bytes that we still
		 * allow to be stored in the receive queue is represented as
		 * 'delta'.  We must make sure that 'left' does not ever exceed
		 * 'delta' while acknowledging as many bytes as possible under
		 * that rule.
		 */
		left = TCP_WND - tcp->tcp_rcv.tr_unacked;
		delta = rcvbuf - tcp->tcp_rcv.tr_len;

		if (left < delta) {
			ack = delta - left;

			if (ack > tcp->tcp_rcv.tr_unacked)
				ack = tcp->tcp_rcv.tr_unacked;

			tcp_recved(tcp->tcp_pcb, ack);

			tcp->tcp_rcv.tr_unacked -= ack;

			assert(tcp->tcp_rcv.tr_len + TCP_WND -
			    tcp->tcp_rcv.tr_unacked <= rcvbuf);
		}
	}
}

/*
 * Attempt to merge two consecutive underfilled buffers in the receive queue of
 * a TCP socket, freeing up one of the two buffers as a result.  The first
 * (oldest) buffer is 'ptail', and the pointer to this buffer is stored at
 * 'pnext'.  The second (new) buffer is 'pbuf', which is already attached to
 * the first buffer.  The second buffer may be followed by additional buffers
 * with even more new data.  Return TRUE if buffers have been merged, in which
 * case the pointer at 'pnext' may have changed, and no assumptions should be
 * made about whether 'ptail' and 'pbuf' still exist in any form.  Return FALSE
 * if no merging was necessary or if no new buffer could be allocated.
 */
static int
tcpsock_try_merge(struct pbuf **pnext, struct pbuf * ptail, struct pbuf * pbuf)
{
	struct pbuf *pnew;

	assert(*pnext == ptail);
	assert(ptail->next == pbuf);

	/*
	 * Unfortunately, we cannot figure out what kind of pbuf we were given
	 * by the lower layers, so we cannot merge two buffers without first
	 * allocating a third.  Once we have done that, though, we can easily
	 * merge more into that new buffer.  For now we use the following
	 * policies:
	 *
	 * 1. if two consecutive lwIP-provided buffers are both used less than
	 *    half the size of a full buffer, try to allocate a new buffer and
	 *    copy both lwIP-provided buffers into that new buffer, freeing up
	 *    the pair afterwards;
	 * 2. if the tail buffer on the chain is allocated by us and not yet
	 *    full, and the next buffer's contents can be added to the tail
	 *    buffer in their entirety, do just that.
	 *
	 * Obviously there is a trade-off between the performance overhead of
	 * copying and the resource overhead of keeping less-than-full buffers
	 * on the receive queue, but this policy should both keep actual memory
	 * usage to no more than twice the receive queue length and prevent
	 * excessive copying.  The policy deliberately performs more aggressive
	 * merging into a buffer that we allocated ourselves.
	 */
	if (ptail->tot_len <= MEMPOOL_BUFSIZE / 2 &&
	    pbuf->len <= MEMPOOL_BUFSIZE / 2) {
		/*
		 * Case #1.
		 */
		assert(ptail->tot_len == ptail->len);
		assert(pbuf->tot_len == pbuf->len);

		pnew = tcpsock_alloc_buf();
		if (pnew == NULL)
			return FALSE;

		memcpy(pnew->payload, ptail->payload, ptail->len);
		memcpy((char *)pnew->payload + ptail->len, pbuf->payload,
		    pbuf->len);
		pnew->len = ptail->len + pbuf->len;
		assert(pnew->len <= pnew->tot_len);

		pnew->next = pbuf->next;
		/* For now, we need not inherit any flags from either pbuf. */

		*pnext = pnew;

		/* One allocated, two about to be deallocated. */
		assert(tcpsock_recvbufs > 0);
		tcpsock_recvbufs--;

		tcpsock_free_buf(ptail);
		tcpsock_free_buf(pbuf);

		return TRUE;
	} else if (ptail->tot_len - ptail->len >= pbuf->len) {
		/*
		 * Case #2.
		 */
		memcpy((char *)ptail->payload + ptail->len, pbuf->payload,
		    pbuf->len);

		ptail->len += pbuf->len;

		ptail->next = pbuf->next;

		assert(tcpsock_recvbufs > 0);
		tcpsock_recvbufs--;

		tcpsock_free_buf(pbuf);

		return TRUE;
	} else
		return FALSE;
}

/*
 * Callback from lwIP.  New data or flags have been received on a TCP socket.
 */
static err_t
tcpsock_event_recv(void * arg, struct tcp_pcb * pcb __unused,
	struct pbuf * pbuf, err_t err)
{
	struct tcpsock *tcp = (struct tcpsock *)arg;
	struct pbuf *ptail, **pprevp;
	size_t len;

	assert(tcp != NULL);
	assert(pcb == tcp->tcp_pcb);

	/*
	 * lwIP should never provide anything other than ERR_OK in 'err', and
	 * it is not clear what we should do if it would.  If lwIP ever changes
	 * in this regard, we will likely have to change this code accordingly.
	 */
	if (err != ERR_OK)
		panic("TCP receive event with error: %d", err);

	/* If the given buffer is NULL, we have received a FIN. */
	if (pbuf == NULL) {
		tcpsock_set_flag(tcp, TCPF_RCVD_FIN);

		/* Userland may now receive EOF. */
		if (!tcpsock_is_shutdown(tcp, SFL_SHUT_RD))
			sockevent_raise(tcpsock_get_sock(tcp), SEV_RECV);

		/*
		 * If we were in the process of closing the socket, and we
		 * receive a FIN before our FIN got acknowledged, we close the
		 * socket anyway, as described in tcpsock_close().  However, if
		 * there is still unacknowledged outgoing data or we did not
		 * even manage to send our FIN yet, hold off closing the socket
		 * for now.
		 */
		if (tcpsock_may_close(tcp))
			(void)tcpsock_finish_close(tcp);

		return ERR_OK;
	}

	/*
	 * If the socket is being closed, receiving new data should cause a
	 * reset.
	 */
	if (sockevent_is_closing(tcpsock_get_sock(tcp))) {
		tcpsock_pcb_abort(tcp);

		(void)tcpsock_cleanup(tcp, TRUE /*may_free*/);
		/* Do not touch the socket object anymore! */

		pbuf_free(pbuf);

		return ERR_ABRT;
	}

	/*
	 * If the socket has already been shut down for reading, discard the
	 * incoming data and do nothing else.
	 */
	if (tcpsock_is_shutdown(tcp, SFL_SHUT_RD)) {
		tcp_recved(tcp->tcp_pcb, pbuf->tot_len);

		pbuf_free(pbuf);

		return ERR_OK;
	}

	/*
	 * We deliberately ignore the PBUF_FLAG_PUSH flag.  This flag would
	 * enable the receive functionality to delay delivering "un-pushed"
	 * data to applications.  The implementation of this scheme could track
	 * the amount of data up to and including the last-pushed segment using
	 * a "tr_push_len" field or so.  Deciding when to deliver "un-pushed"
	 * data after all is a bit tricker though.  As far as I can tell, the
	 * BSDs do not implement anything like that.  Windows does, and this
	 * results in interaction problems with even more lightweight TCP/IP
	 * stacks that do not send the TCP PSH flag.  Currently, there is no
	 * obvious benefit for us to support delaying data delivery like that.
	 * In addition, testing its implementation reliably would be difficult.
	 */

	len = (size_t)pbuf->tot_len;

	/*
	 * Count the number of buffers that are now owned by us.  The new total
	 * of buffers owned by us must not exceed the size of the memory pool.
	 * Any more would indicate an accounting error.  Note that
	 * tcpsock_recvbufs is currently used for debugging only!
	 */
	tcpsock_recvbufs += pbuf_clen(pbuf);
	assert(tcpsock_recvbufs < mempool_cur_buffers());

	/*
	 * The pre-tail pointer points to whatever is pointing to the tail
	 * buffer.  The latter pointer may be the 'tr_head' field in our
	 * tcpsock structure, or the 'next' field in the penultimate buffer,
	 * or NULL if there are currently no buffers on the receive queue.
	 */
	if ((pprevp = tcp->tcp_rcv.tr_pre_tailp) != NULL) {
		ptail = *pprevp;

		assert(ptail != NULL);
		assert(ptail->next == NULL);
		assert(tcp->tcp_rcv.tr_head != NULL);

		ptail->next = pbuf;
		pbuf->tot_len = pbuf->len;	/* to help freeing on merges */

		if (tcpsock_try_merge(pprevp, ptail, pbuf)) {
			ptail = *pprevp;
			pbuf = ptail->next;
		}

		if (pbuf != NULL)
			pprevp = &ptail->next;
	} else {
		assert(tcp->tcp_rcv.tr_head == NULL);
		assert(tcp->tcp_rcv.tr_head_off == 0);

		tcp->tcp_rcv.tr_head = pbuf;

		pprevp = &tcp->tcp_rcv.tr_head;
	}

	/*
	 * Chop up the chain into individual buffers.  This is necessary as we
	 * overload 'tot_len' to mean "space available in the buffer", as we
	 * want for buffers allocated by us as part of buffer merges.  Also get
	 * a pointer to the pointer to the new penultimate tail buffer.  Due to
	 * merging, the chain may already be empty by now, though.
	 */
	if (pbuf != NULL) {
		for (; pbuf->next != NULL; pbuf = pbuf->next) {
			pbuf->tot_len = pbuf->len;

			pprevp = &pbuf->next;
		}
		assert(pbuf->len == pbuf->tot_len);
	}

	assert(*pprevp != NULL);
	assert((*pprevp)->next == NULL);
	tcp->tcp_rcv.tr_pre_tailp = pprevp;

	tcp->tcp_rcv.tr_len += len;
	tcp->tcp_rcv.tr_unacked += len;

	assert(tcp->tcp_rcv.tr_unacked <= TCP_WND);

	/*
	 * Note that tr_len may now exceed the receive buffer size in the
	 * highly exceptional case that the user is adjusting the latter after
	 * the socket had already received data.
	 */

	/* See if we can immediately acknowledge some or all of the data. */
	tcpsock_ack_recv(tcp);

	/* Also wake up any receivers now. */
	sockevent_raise(tcpsock_get_sock(tcp), SEV_RECV);

	return ERR_OK;
}

/*
 * Callback from lwIP.  The PCB corresponding to the socket identified by 'arg'
 * has been closed by lwIP, with the reason specified in 'err': either the
 * connection has been aborted locally (ERR_ABRT), it has been reset by the
 * remote end (ERR_RST), or it is closed due to state transitions (ERR_CLSD).
 */
static void
tcpsock_event_err(void * arg, err_t err)
{
	struct tcpsock *tcp = (struct tcpsock *)arg;
	int r;

	assert(tcp != NULL);
	assert(tcp->tcp_pcb != NULL);
	assert(err != ERR_OK);

	/* The original PCB is now gone, or will be shortly. */
	tcp->tcp_pcb = NULL;

	/*
	 * Clean up the socket.  As a result it may be freed, in which case we
	 * must not touch it anymore.  No need to return ERR_ABRT from here, as
	 * the PCB has been aborted already.
	 */
	if (tcpsock_cleanup(tcp, TRUE /*may_free*/))
		return;

	if (err == ERR_CLSD) {
		/*
		 * We may get here if the socket is shut down for writing and
		 * we already received a FIN from the remote side, thus putting
		 * the socket in LAST_ACK state, and we receive that last
		 * acknowledgment.  There is nothing more we need to do.
		 *
		 * We will never get here in the other case that ERR_CLSD is
		 * raised, which is when the socket is reset because of
		 * unacknowledged data while closing: we handle the
		 * reset-on-ACK case ourselves in tcpsock_close(), and the
		 * socket is in closing state after that.
		 */
		assert(tcpsock_is_shutdown(tcp, SFL_SHUT_WR));
		assert(tcpsock_get_flags(tcp) & TCPF_RCVD_FIN);
	} else {
		/*
		 * Anything else should be an error directly from lwIP;
		 * currently either ERR_ABRT and ERR_RST.  Covert it to a
		 * regular error and set it on the socket.  Doing so will also
		 * raise the appropriate events.
		 */
		/*
		 * Unfortunately, lwIP is not throwing accurate errors even
		 * when it can.  We convert some errors to reflect more
		 * accurately the most likely cause.
		 *
		 * TODO: fix lwIP in this regard..
		 */
		r = util_convert_err(err);

		if (tcpsock_get_flags(tcp) & TCPF_CONNECTING) {
			switch (err) {
			case ERR_ABRT:	r = ETIMEDOUT;		break;
			case ERR_RST:	r = ECONNREFUSED;	break;
			}
		}

		sockevent_set_error(tcpsock_get_sock(tcp), r);
	}
}

/*
 * Callback from lwIP.  Perform regular checks on a TCP socket.  This function
 * is called one per five seconds on connected sockets, and twice per second on
 * closing sockets.
 */
static err_t
tcpsock_event_poll(void * arg, struct tcp_pcb * pcb __unused)
{
	struct tcpsock *tcp = (struct tcpsock *)arg;
	err_t err;
	int r;

	assert(tcp != NULL);
	assert(pcb == tcp->tcp_pcb);

	/*
	 * If we ended up running out of buffers earlier, try resuming any send
	 * requests now, both for enqueuing TCP data with lwIP and for user
	 * requests.
	 */
	if (tcpsock_get_flags(tcp) & (TCPF_FULL | TCPF_OOM)) {
		tcpsock_clear_flag(tcp, TCPF_FULL);
		tcpsock_clear_flag(tcp, TCPF_OOM);

		/* See if we can enqueue more data with lwIP. */
		if (tcpsock_pcb_enqueue(tcp)) {
			/* In some cases, we can now close the PCB. */
			if (tcpsock_may_close(tcp)) {
				(void)tcpsock_finish_close(tcp);
				/*
				 * The PCB is definitely gone here, and the
				 * entire socket object may be gone now too.
				 * Do not touch either anymore!
				 */

				return ERR_OK;
			}

			/*
			 * If actually sending the data fails, the PCB will be
			 * gone, and the socket object may be gone as well.  Do
			 * not touch either anymore in that case!
			 */
			if (tcpsock_pcb_send(tcp, TRUE /*raise_error*/) != OK)
				return ERR_ABRT;
		}

		/*
		 * If we ran out of buffers earlier, it may be possible to take
		 * in more data from a user process now, even if we did not
		 * manage to enqueue any more pending data with lwIP.
		 */
		sockevent_raise(tcpsock_get_sock(tcp), SEV_SEND);

		assert(tcp->tcp_pcb != NULL);
	} else if (tcp->tcp_snd.ts_unsent != NULL &&
	    tcp->tcp_snd.ts_unsent_off < tcp->tcp_snd.ts_unsent->len) {
		/*
		 * If the send buffer is full, we will no longer call
		 * tcp_output(), which means we may also miss out on fatal
		 * errors that would otherwise kill the connection (e.g., no
		 * route).  As a result, the connection may erroneously
		 * continue to exist for a long time.  To avoid this, we call
		 * tcp_output() every once in a while when there are still
		 * unsent data.
		 */
		err = tcp_output(tcp->tcp_pcb);

		if (err != ERR_OK && err != ERR_MEM) {
			tcpsock_pcb_abort(tcp);

			if (!tcpsock_cleanup(tcp, TRUE /*may_free*/)) {
				r = util_convert_err(err);

				sockevent_set_error(tcpsock_get_sock(tcp), r);
			}
			/* Otherwise do not touch the socket object anymore! */

			return ERR_ABRT;
		}
	}

	/*
	 * If we are closing the socket, and we sent a FIN, see if the FIN got
	 * acknowledged.  If so, finish closing the socket.  Unfortunately, we
	 * can perform this check by polling only.  TODO: change lwIP..
	 */
	if (sockevent_is_closing(tcpsock_get_sock(tcp)) &&
	    (tcpsock_get_flags(tcp) & TCPF_SENT_FIN) &&
	    tcp->tcp_pcb->unsent == NULL && tcp->tcp_pcb->unacked == NULL) {
		assert(tcp->tcp_snd.ts_len == 0);

		tcpsock_finish_close(tcp);
	}

	return ERR_OK;
}

/*
 * Bind a TCP socket to a local address.
 */
static int
tcpsock_bind(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	ip_addr_t ipaddr;
	uint16_t port;
	err_t err;
	int r;

	if (tcp->tcp_pcb == NULL || tcp->tcp_pcb->state != CLOSED)
		return EINVAL;

	if ((r = ipsock_get_src_addr(tcpsock_get_ipsock(tcp), addr, addr_len,
	    user_endpt, &tcp->tcp_pcb->local_ip, tcp->tcp_pcb->local_port,
	    FALSE /*allow_mcast*/, &ipaddr, &port)) != OK)
		return r;

	err = tcp_bind(tcp->tcp_pcb, &ipaddr, port);

	return util_convert_err(err);
}

/*
 * Callback from lwIP.  A new connection 'pcb' has arrived on the listening
 * socket identified by 'arg'.  Note that 'pcb' may be NULL in the case that
 * lwIP could not accept the connection itself.
 */
static err_t
tcpsock_event_accept(void * arg, struct tcp_pcb * pcb, err_t err)
{
	struct tcpsock *tcp = (struct tcpsock *)arg;

	assert(tcp != NULL);
	assert(tcpsock_is_listening(tcp));

	/*
	 * If the given PCB is NULL, then lwIP ran out of memory allocating a
	 * PCB for the new connection.  There is nothing we can do with that
	 * information.  Also check 'err' just to make sure.
	 */
	if (pcb == NULL || err != OK)
		return ERR_OK;

	/*
	 * The TCP socket is the listening socket, but the PCB is for the
	 * incoming connection.
	 */
	if (tcpsock_clone(tcp, pcb) != OK) {
		/*
		 * We could not allocate the resources necessary to accept the
		 * connection.  Abort it immediately.
		 */
		tcp_abort(pcb);

		return ERR_ABRT;
	}

	/*
	 * The connection has not yet been accepted, and thus should still be
	 * considered on the listen queue.
	 */
	tcp_backlog_delayed(pcb);

	/* Set the callback functions. */
	tcp_recv(pcb, tcpsock_event_recv);
	tcp_sent(pcb, tcpsock_event_sent);
	tcp_err(pcb, tcpsock_event_err);
	tcp_poll(pcb, tcpsock_event_poll, TCP_POLL_REG_INTERVAL);

	sockevent_raise(tcpsock_get_sock(tcp), SEV_ACCEPT);

	return ERR_OK;
}

/*
 * Put a TCP socket in listening mode.
 */
static int
tcpsock_listen(struct sock * sock, int backlog)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	struct tcp_pcb *pcb;
	err_t err;

	/* The maximum backlog value must not exceed its field size. */
	assert(SOMAXCONN <= UINT8_MAX);

	/*
	 * Allow only CLOSED sockets to enter listening mode.  If the socket
	 * was already in listening mode, allow its backlog value to be
	 * updated, even if it was shut down already (making this a no-op).
	 */
	if (!tcpsock_is_listening(tcp) &&
	    (tcp->tcp_pcb == NULL || tcp->tcp_pcb->state != CLOSED))
		return EINVAL;

	/*
	 * If the socket was not already in listening mode, put it in that mode
	 * now.  That involves switching PCBs as lwIP attempts to save memory
	 * by replacing the original PCB with a smaller one.  If the socket was
	 * already in listening mode, simply update its backlog value--this has
	 * no effect on the sockets already in the backlog.
	 */
	if (!tcpsock_is_listening(tcp)) {
		assert(tcp->tcp_pcb != NULL);

		/*
		 * If the socket has not been bound to a port yet, do that
		 * first.  This does mean that the listen call may fail with
		 * side effects, but that is acceptable in this case.
		 */
		if (tcp->tcp_pcb->local_port == 0) {
			err = tcp_bind(tcp->tcp_pcb, &tcp->tcp_pcb->local_ip,
			    0 /*port*/);

			if (err != ERR_OK)
				return util_convert_err(err);
		}

		/*
		 * Clear the argument on the PCB that is about to be replaced,
		 * because if we do not, once the PCB is reused (which does not
		 * clear the argument), we might get weird events.  Do this
		 * before the tcp_listen() call, because we should no longer
		 * access the old PCB afterwards (even if we can).
		 */
		tcp_arg(tcp->tcp_pcb, NULL);

		pcb = tcp_listen_with_backlog_and_err(tcp->tcp_pcb, backlog,
		    &err);

		if (pcb == NULL) {
			tcp_arg(tcp->tcp_pcb, tcp); /* oops, undo. */

			return util_convert_err(err);
		}

		tcp_arg(pcb, tcp);
		tcp->tcp_pcb = pcb;

		tcp_accept(pcb, tcpsock_event_accept);

		/* Initialize the queue head for sockets pending acceptance. */
		TAILQ_INIT(&tcp->tcp_queue.tq_head);
	} else if (tcp->tcp_pcb != NULL)
		tcp_backlog_set(tcp->tcp_pcb, backlog);

	return OK;
}

/*
 * Callback from lwIP.  A socket connection attempt has succeeded.  Note that
 * failed socket events will trigger the tcpsock_event_err() callback instead.
 */
static err_t
tcpsock_event_connected(void * arg, struct tcp_pcb * pcb __unused, err_t err)
{
	struct tcpsock *tcp = (struct tcpsock *)arg;

	assert(tcp != NULL);
	assert(pcb == tcp->tcp_pcb);
	assert(tcpsock_get_flags(tcp) & TCPF_CONNECTING);

	/*
	 * If lwIP ever changes so that this callback is called for connect
	 * failures as well, then we need to change the code here accordingly.
	 */
	if (err != ERR_OK)
		panic("TCP connected event with error: %d", err);

	tcpsock_clear_flag(tcp, TCPF_CONNECTING);

	sockevent_raise(tcpsock_get_sock(tcp), SEV_CONNECT | SEV_SEND);

	return ERR_OK;
}

/*
 * Connect a TCP socket to a remote address.
 */
static int
tcpsock_connect(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	ip_addr_t dst_addr;
	uint16_t dst_port;
	err_t err;
	int r;

	/*
	 * Listening sockets may not have a PCB, so we use higher-level flags
	 * to throw the correct error code for those instead.
	 */
	if (tcpsock_is_listening(tcp))
		return EOPNOTSUPP;

	/*
	 * If there is no longer any PCB, we obviously cannot perform the
	 * connection, but POSIX is not clear on which error to return.  We
	 * copy NetBSD's.
	 */
	if (tcp->tcp_pcb == NULL)
		return EINVAL;

	/*
	 * The only state from which a connection can be initiated, is CLOSED.
	 * Some of the other states require distinct error codes, though.
	 */
	switch (tcp->tcp_pcb->state) {
	case CLOSED:
		break;
	case SYN_SENT:
		return EALREADY;
	case LISTEN:
		assert(0); /* we just checked.. */
	default:
		return EISCONN;
	}

	/*
	 * Get the destination address, and attempt to start connecting.  If
	 * the socket was not bound before, or it was bound to a port only,
	 * then lwIP will select a source address for us.  We cannot do this
	 * ourselves even if we wanted to: it is impossible to re-bind a TCP
	 * PCB in the case it was previously bound to a port only.
	 */
	if ((r = ipsock_get_dst_addr(tcpsock_get_ipsock(tcp), addr, addr_len,
	    &tcp->tcp_pcb->local_ip, &dst_addr, &dst_port)) != OK)
		return r;

	err = tcp_connect(tcp->tcp_pcb, &dst_addr, dst_port,
	    tcpsock_event_connected);

	/*
	 * Note that various tcp_connect() error cases will leave the PCB with
	 * a newly set local and remote IP address anyway.  We should be
	 * careful not to rely on the addresses being as they were before.
	 */
	if (err != ERR_OK)
		return util_convert_err(err);

	/* Set the other callback functions. */
	tcp_recv(tcp->tcp_pcb, tcpsock_event_recv);
	tcp_sent(tcp->tcp_pcb, tcpsock_event_sent);
	tcp_err(tcp->tcp_pcb, tcpsock_event_err);
	tcp_poll(tcp->tcp_pcb, tcpsock_event_poll, TCP_POLL_REG_INTERVAL);

	/*
	 * Set a flag so that we can correct lwIP's error codes in case the
	 * connection fails.
	 */
	tcpsock_set_flag(tcp, TCPF_CONNECTING);

	return SUSPEND;
}

/*
 * Test whether any new connections are pending on a listening TCP socket.
 */
static int
tcpsock_test_accept(struct sock * sock)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;

	/* Is this socket in listening mode at all? */
	if (!tcpsock_is_listening(tcp))
		return EINVAL;

	/* Are there any connections to accept right now? */
	if (!TAILQ_EMPTY(&tcp->tcp_queue.tq_head))
		return OK;

	/* If the socket has been shut down, we return ECONNABORTED. */
	if (tcp->tcp_pcb == NULL)
		return ECONNABORTED;

	/* Otherwise, wait for a new connection first. */
	return SUSPEND;
}

/*
 * Accept a connection on a listening TCP socket, creating a new TCP socket.
 */
static sockid_t
tcpsock_accept(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len, endpoint_t user_endpt __unused,
	struct sock ** newsockp)
{
	struct tcpsock *listener = (struct tcpsock *)sock;
	struct tcpsock *tcp;
	int r;

	if ((r = tcpsock_test_accept(sock)) != OK)
		return r;
	/* Below, we must not assume that the listener has a PCB. */

	tcp = TAILQ_FIRST(&listener->tcp_queue.tq_head);
	assert(tcp->tcp_listener == listener);
	assert(tcp->tcp_pcb != NULL);

	TAILQ_REMOVE(&listener->tcp_queue.tq_head, tcp, tcp_queue.tq_next);
	tcp->tcp_listener = NULL;

	tcp_backlog_accepted(tcp->tcp_pcb);

	ipsock_put_addr(tcpsock_get_ipsock(tcp), addr, addr_len,
	    &tcp->tcp_pcb->remote_ip, tcp->tcp_pcb->remote_port);

	/*
	 * Set 'newsockp' to NULL so that libsockevent knows we already cloned
	 * the socket, and it must not be reinitialized anymore.
	 */
	*newsockp = NULL;
	return tcpsock_get_id(tcp);
}

/*
 * Perform preliminary checks on a send request.
 */
static int
tcpsock_pre_send(struct sock * sock, size_t len __unused,
	socklen_t ctl_len __unused, const struct sockaddr * addr __unused,
	socklen_t addr_len __unused, endpoint_t user_endpt __unused, int flags)
{

	/*
	 * Reject calls with unknown flags.  Since libsockevent strips out the
	 * flags it handles itself here, we only have to test for ones we can
	 * not handle.  Currently, there are no send flags that we support.
	 */
	if (flags != 0)
		return EOPNOTSUPP;

	return OK;
}

/*
 * Test whether the given number of data bytes can be sent on a TCP socket.
 */
static int
tcpsock_test_send(struct sock * sock, size_t min)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	size_t sndbuf;

	if (tcp->tcp_pcb == NULL)
		return EPIPE;

	switch (tcp->tcp_pcb->state) {
	case CLOSED:			/* new */
	case LISTEN:			/* listening */
		return ENOTCONN;
	case SYN_SENT:			/* connecting */
	case SYN_RCVD:			/* simultaneous open, maybe someday? */
		return SUSPEND;
	case ESTABLISHED:		/* connected */
	case CLOSE_WAIT:		/* closed remotely */
		break;
	default:			/* shut down locally */
		assert(tcpsock_is_shutdown(tcp, SFL_SHUT_WR));
		return EPIPE;
	}

	sndbuf = tcpsock_get_sndbuf(tcp);
	if (min > sndbuf)
		min = sndbuf;

	if (tcp->tcp_snd.ts_len + min > sndbuf)
		return SUSPEND;
	else
		return OK;
}

/*
 * Send data on a TCP socket.
 */
static int
tcpsock_send(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * offp, const struct sockdriver_data * ctl __unused,
	socklen_t ctl_len __unused, socklen_t * ctl_off __unused,
	const struct sockaddr * addr __unused, socklen_t addr_len __unused,
	endpoint_t user_endpt __unused, int flags __unused, size_t min)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	struct pbuf *ptail, *pfirst, *pnext, *plast;
	size_t off, tail_off, chunk, left, sndbuf;
	int r;

	if ((r = tcpsock_test_send(sock, min)) != OK)
		return r;

	if (len == 0)
		return OK;	/* nothing to do */

	sndbuf = tcpsock_get_sndbuf(tcp);
	if (min > sndbuf)
		min = sndbuf;
	assert(min > 0);

	assert(sndbuf > tcp->tcp_snd.ts_len);
	left = sndbuf - tcp->tcp_snd.ts_len;
	if (left > len)
		left = len;

	/*
	 * First see if we can fit any more data in the current tail buffer.
	 * If so, we set 'ptail' to point to it and 'tail_off' to the previous
	 * length of the tail buffer, while optimistically extending it to
	 * include the new data.  If not, we set them to NULL/0.
	 */
	if ((ptail = tcp->tcp_snd.ts_tail) != NULL &&
	    ptail->len < ptail->tot_len) {
		assert(ptail->len > 0);
		tail_off = (size_t)ptail->len;

		/*
		 * Optimistically extend the head buffer to include whatever
		 * fits in it.  This is needed for util_copy_data().
		 */
		assert(ptail->tot_len > ptail->len);
		off = (size_t)ptail->tot_len - (size_t)ptail->len;
		if (off > left)
			off = left;
		ptail->len += off;
	} else {
		ptail = NULL;
		tail_off = 0;
		off = 0;
	}

	/*
	 * Then, if there is more to send, allocate new buffers as needed.  If
	 * we run out of memory, work with whatever we did manage to grab.
	 */
	pfirst = NULL;
	plast = NULL;
	while (off < left) {
		if (tcpsock_sendbufs >= TCP_MAX_SENDBUFS ||
		    (pnext = tcpsock_alloc_buf()) == NULL) {
			/*
			 * Chances are that we will end up suspending this send
			 * request because of being out of buffers.  We try to
			 * resume such requests from the polling function.
			 */
			tcpsock_set_flag(tcp, TCPF_OOM);

			break;
		}

		tcpsock_sendbufs++;

		if (pfirst == NULL)
			pfirst = pnext;
		else
			plast->next = pnext;
		plast = pnext;

		chunk = (size_t)pnext->tot_len;
		if (chunk > left - off)
			chunk = left - off;
		pnext->len = chunk;
		off += chunk;
	}

	/*
	 * Copy in the data and continue, unless we did not manage to find
	 * enough space to even meet the low send watermark, in which case we
	 * undo any allocation and suspend the call until later.
	 */
	if (off >= min) {
		/*
		 * Optimistically attach the new buffers to the tail, also for
		 * util_copy_data().  We undo all this if the copy fails.
		 */
		if (ptail != NULL) {
			ptail->next = pfirst;

			pnext = ptail;
		} else
			pnext = pfirst;

		assert(pnext != NULL);

		r = util_copy_data(data, off, *offp, pnext, tail_off,
		    TRUE /*copy_in*/);
	} else
		r = SUSPEND;

	if (r != OK) {
		/* Undo the modifications made so far. */
		while (pfirst != NULL) {
			pnext = pfirst->next;

			assert(tcpsock_sendbufs > 0);
			tcpsock_sendbufs--;

			tcpsock_free_buf(pfirst);

			pfirst = pnext;
		}

		if (ptail != NULL) {
			ptail->next = NULL;

			ptail->len = tail_off;
		}

		return r;
	}

	/* Attach the new buffers, if any, to the buffer tail. */
	if (pfirst != NULL) {
		if ((ptail = tcp->tcp_snd.ts_tail) != NULL) {
			assert(ptail->len == ptail->tot_len);

			/*
			 * Due to our earlier optimistic modifications, this
			 * may or may not be redundant.
			 */
			ptail->next = pfirst;
		}

		assert(plast != NULL);
		tcp->tcp_snd.ts_tail = plast;

		if (tcp->tcp_snd.ts_head == NULL) {
			tcp->tcp_snd.ts_head = pfirst;
			assert(tcp->tcp_snd.ts_head_off == 0);
		}
		if (tcp->tcp_snd.ts_unsent == NULL) {
			tcp->tcp_snd.ts_unsent = pfirst;
			assert(tcp->tcp_snd.ts_unsent_off == 0);
		}
	}

	tcp->tcp_snd.ts_len += off;

	/*
	 * See if we can send any of the data we just enqueued.  The socket is
	 * still open as we are still processing a call from userland on it;
	 * this saves us from having to deal with the cases that the following
	 * calls end up freeing the socket object.
	 */
	if (tcpsock_pcb_enqueue(tcp) &&
	    (r = tcpsock_pcb_send(tcp, FALSE /*raise_error*/)) != OK) {
		/*
		 * That did not go well.  Return the error immediately if we
		 * had not made any progress earlier.  Otherwise, return our
		 * partial progress and leave the error to be picked up later.
		 */
		if (*offp > 0) {
			sockevent_set_error(tcpsock_get_sock(tcp), r);

			return OK;
		} else
			return r;
	}

	*offp += off;
	return (off < len) ? SUSPEND : OK;
}

/*
 * Perform preliminary checks on a receive request.
 */
static int
tcpsock_pre_recv(struct sock * sock __unused, endpoint_t user_endpt __unused,
	int flags)
{

	/*
	 * Reject calls with unknown flags.  Since libsockevent strips out the
	 * flags it handles itself here, we only have to test for ones we can
	 * not handle.
	 */
	if ((flags & ~(MSG_PEEK | MSG_WAITALL)) != 0)
		return EOPNOTSUPP;

	return OK;
}

/*
 * Return TRUE if receive calls may wait for more data to come in on the
 * connection, or FALSE if we already know that that is not going to happen.
 */
static int
tcpsock_may_wait(struct tcpsock * tcp)
{

	return (tcp->tcp_pcb != NULL &&
	    !(tcpsock_get_flags(tcp) & TCPF_RCVD_FIN));
}

/*
 * Test whether data can be received on a TCP socket, and if so, how many bytes
 * of data.
 */
static int
tcpsock_test_recv(struct sock * sock, size_t min, size_t * size)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	int may_wait;

	/* If there is and never was a connection, refuse the call at all. */
	if (tcp->tcp_pcb != NULL && (tcp->tcp_pcb->state == CLOSED ||
	    tcp->tcp_pcb->state == LISTEN))
		return ENOTCONN;

	/*
	 * If we are certain that no more data will come in later, ignore the
	 * low receive watermark.  Otherwise, bound it to the size of the
	 * receive buffer, or receive calls may block forever.
	 */
	if (!(may_wait = tcpsock_may_wait(tcp)))
		min = 1;
	else if (min > tcpsock_get_rcvbuf(tcp))
		min = tcpsock_get_rcvbuf(tcp);

	if (tcp->tcp_rcv.tr_len >= min) {
		if (size != NULL)
			*size = tcp->tcp_rcv.tr_len;

		return OK;
	}

	return (may_wait) ? SUSPEND : SOCKEVENT_EOF;
}

/*
 * Receive data on a TCP socket.
 */
static int
tcpsock_recv(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * offp, const struct sockdriver_data * ctl __unused,
	socklen_t ctl_len __unused, socklen_t * ctl_off __unused,
	struct sockaddr * addr __unused, socklen_t * addr_len __unused,
	endpoint_t user_endpt __unused, int flags, size_t min,
	int * rflags __unused)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	struct pbuf *ptail;
	size_t off, left;
	int r;

	/* See if we can receive at all, and if so, how much at most. */
	if ((r = tcpsock_test_recv(sock, min, NULL)) != OK)
		return r;

	if (len == 0)
		return OK;	/* nothing to do */

	off = tcp->tcp_rcv.tr_len;
	if (off > len)
		off = len;

	assert(tcp->tcp_rcv.tr_head != NULL);
	assert(tcp->tcp_rcv.tr_head_off < tcp->tcp_rcv.tr_head->len);

	/* Copy out the data to the caller. */
	if ((r = util_copy_data(data, off, *offp, tcp->tcp_rcv.tr_head,
	    tcp->tcp_rcv.tr_head_off, FALSE /*copy_in*/)) != OK)
		return r;

	/* Unless peeking, remove the data from the receive queue. */
	if (!(flags & MSG_PEEK)) {
		left = off;

		/* Dequeue and free as many entire buffers as possible. */
		while ((ptail = tcp->tcp_rcv.tr_head) != NULL &&
		    left >= (size_t)ptail->len - tcp->tcp_rcv.tr_head_off) {
			left -= (size_t)ptail->len - tcp->tcp_rcv.tr_head_off;

			tcp->tcp_rcv.tr_head = ptail->next;
			tcp->tcp_rcv.tr_head_off = 0;

			if (tcp->tcp_rcv.tr_head == NULL)
				tcp->tcp_rcv.tr_pre_tailp = NULL;
			else if (tcp->tcp_rcv.tr_pre_tailp == &ptail->next)
				tcp->tcp_rcv.tr_pre_tailp =
				    &tcp->tcp_rcv.tr_head;

			assert(tcpsock_recvbufs > 0);
			tcpsock_recvbufs--;

			tcpsock_free_buf(ptail);
		}

		/*
		 * If only part of the (new) head buffer is consumed, adjust
		 * the saved offset into that buffer.
		 */
		if (left > 0) {
			assert(tcp->tcp_rcv.tr_head != NULL);
			assert((size_t)tcp->tcp_rcv.tr_head->len -
			    tcp->tcp_rcv.tr_head_off > left);

			tcp->tcp_rcv.tr_head_off += left;
		}

		tcp->tcp_rcv.tr_len -= off;

		if (tcp->tcp_rcv.tr_head != NULL) {
			assert(tcp->tcp_rcv.tr_pre_tailp != NULL);
			assert(tcp->tcp_rcv.tr_len > 0);
		} else {
			assert(tcp->tcp_rcv.tr_pre_tailp == NULL);
			assert(tcp->tcp_rcv.tr_len == 0);
		}

		/*
		 * The receive buffer has shrunk, so there may now be space to
		 * receive more data.
		 */
		if (tcp->tcp_pcb != NULL)
			tcpsock_ack_recv(tcp);
	} else
		flags &= ~MSG_WAITALL; /* for the check below */

	/* Advance the current copy position, and see if we are done. */
	*offp += off;
	if ((flags & MSG_WAITALL) && off < len && tcpsock_may_wait(tcp))
		return SUSPEND;
	else
		return OK;
}

/*
 * Update the set of flag-type socket options on a TCP socket.
 */
static void
tcpsock_setsockmask(struct sock * sock, unsigned int mask)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;

	if (tcp->tcp_pcb == NULL)
		return;

	if (mask & SO_REUSEADDR)
		ip_set_option(tcp->tcp_pcb, SOF_REUSEADDR);
	else
		ip_reset_option(tcp->tcp_pcb, SOF_REUSEADDR);

	if (mask & SO_KEEPALIVE)
		ip_set_option(tcp->tcp_pcb, SOF_KEEPALIVE);
	else
		ip_reset_option(tcp->tcp_pcb, SOF_KEEPALIVE);
}

/*
 * Prepare a helper structure for IP-level option processing.
 */
static void
tcpsock_get_ipopts(struct tcpsock * tcp, struct ipopts * ipopts)
{

	ipopts->local_ip = &tcp->tcp_pcb->local_ip;
	ipopts->remote_ip = &tcp->tcp_pcb->remote_ip;
	ipopts->tos = &tcp->tcp_pcb->tos;
	ipopts->ttl = &tcp->tcp_pcb->ttl;
	ipopts->sndmin = TCP_SNDBUF_MIN;
	ipopts->sndmax = TCP_SNDBUF_MAX;
	ipopts->rcvmin = TCP_RCVBUF_MIN;
	ipopts->rcvmax = TCP_RCVBUF_MAX;
}

/*
 * Set socket options on a TCP socket.
 */
static int
tcpsock_setsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t len)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	struct ipopts ipopts;
	uint32_t uval;
	int r, val;

	if (tcp->tcp_pcb == NULL)
		return ECONNRESET;

	/* Handle TCP-level options. */
	switch (level) {
	case IPPROTO_IPV6:
		switch (name) {
		case IPV6_RECVTCLASS:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			/*
			 * This option is not supported for TCP sockets; it
			 * would not even make sense.  However, named(8)
			 * insists on trying to set it anyway.  We accept the
			 * request but ignore the value, not even returning
			 * what was set through getsockopt(2).
			 */
			return OK;

		case IPV6_FAITH:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			/*
			 * This option is not supported at all, but to save
			 * ourselves from having to remember the current state
			 * for getsockopt(2), we also refuse to enable it.
			 */
			if (val != 0)
				return EINVAL;

			return OK;
		}

		break;

	case IPPROTO_TCP:
		switch (name) {
		case TCP_NODELAY:
			/*
			 * lwIP's listening TCP PCBs do not have this field.
			 * If this ever becomes an issue, we can create our own
			 * shadow flag and do the inheritance ourselves.
			 */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val)
				tcp_nagle_disable(tcp->tcp_pcb);
			else
				tcp_nagle_enable(tcp->tcp_pcb);

			return OK;

		case TCP_KEEPIDLE:
		case TCP_KEEPINTVL:
			/*
			 * lwIP's listening TCP PCBs do not have these fields.
			 */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val == 0)
				return EINVAL;

			/*
			 * The given value is unsigned, but lwIP stores the
			 * value in milliseconds in a uint32_t field, so we
			 * have to limit large values to whatever fits in the
			 * field anyway.
			 */
			if (val < 0 || (uint32_t)val > UINT32_MAX / 1000)
				uval = UINT32_MAX;
			else
				uval = (uint32_t)val * 1000;

			if (name == TCP_KEEPIDLE)
				tcp->tcp_pcb->keep_idle = uval;
			else
				tcp->tcp_pcb->keep_intvl = uval;

			return OK;

		case TCP_KEEPCNT:
			/* lwIP's listening TCP PCBs do not have this field. */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val == 0)
				return EINVAL;

			tcp->tcp_pcb->keep_cnt = (uint32_t)val;

			return OK;
		}

		return EOPNOTSUPP;
	}

	/* Handle all other options at the IP level. */
	tcpsock_get_ipopts(tcp, &ipopts);

	return ipsock_setsockopt(tcpsock_get_ipsock(tcp), level, name, data,
	    len, &ipopts);
}

/*
 * Retrieve socket options on a TCP socket.
 */
static int
tcpsock_getsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t * len)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	struct ipopts ipopts;
	int val;

	if (tcp->tcp_pcb == NULL)
		return ECONNRESET;

	/* Handle TCP-level options. */
	switch (level) {
	case IPPROTO_IPV6:
		switch (name) {
		case IPV6_RECVTCLASS:
		case IPV6_FAITH:
			val = 0;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;

	case IPPROTO_TCP:
		switch (name) {
		case TCP_NODELAY:
			/* lwIP's listening TCP PCBs do not have this field. */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			val = tcp_nagle_disabled(tcp->tcp_pcb);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case TCP_MAXSEG:
			/* lwIP's listening TCP PCBs do not have this field. */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			/* This option is read-only at this time. */
			val = tcp->tcp_pcb->mss;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case TCP_KEEPIDLE:
			/* lwIP's listening TCP PCBs do not have this field. */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			val = (int)(tcp->tcp_pcb->keep_idle / 1000);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case TCP_KEEPINTVL:
			/* lwIP's listening TCP PCBs do not have this field. */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			val = (int)(tcp->tcp_pcb->keep_intvl / 1000);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case TCP_KEEPCNT:
			/* lwIP's listening TCP PCBs do not have this field. */
			if (tcp->tcp_pcb->state == LISTEN)
				return EINVAL;

			val = (int)tcp->tcp_pcb->keep_cnt;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		return EOPNOTSUPP;
	}

	/* Handle all other options at the IP level. */
	tcpsock_get_ipopts(tcp, &ipopts);

	return ipsock_getsockopt(tcpsock_get_ipsock(tcp), level, name, data,
	    len, &ipopts);
}

/*
 * Retrieve the local socket address of a TCP socket.
 */
static int
tcpsock_getsockname(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;

	if (tcp->tcp_pcb == NULL)
		return EINVAL;

	ipsock_put_addr(tcpsock_get_ipsock(tcp), addr, addr_len,
	    &tcp->tcp_pcb->local_ip, tcp->tcp_pcb->local_port);

	return OK;
}

/*
 * Retrieve the remote socket address of a TCP socket.
 */
static int
tcpsock_getpeername(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;

	if (tcp->tcp_pcb == NULL || tcp->tcp_pcb->state == CLOSED ||
	    tcp->tcp_pcb->state == LISTEN || tcp->tcp_pcb->state == SYN_SENT)
		return ENOTCONN;

	ipsock_put_addr(tcpsock_get_ipsock(tcp), addr, addr_len,
	    &tcp->tcp_pcb->remote_ip, tcp->tcp_pcb->remote_port);

	return OK;
}

/*
 * Perform a TCP half-close on a TCP socket.  This operation may not complete
 * immediately due to memory conditions, in which case it will be completed at
 * a later time.
 */
static void
tcpsock_send_fin(struct tcpsock * tcp)
{

	sockevent_set_shutdown(tcpsock_get_sock(tcp), SFL_SHUT_WR);

	/*
	 * Attempt to send the FIN.  If a fatal error occurs as a result, raise
	 * it as an asynchronous error, because this function's callers cannot
	 * do much with it.  That happens to match the way these functions are
	 * used elsewhere.  In any case, as a result, the PCB may be closed.
	 * However, we are never called from a situation where the socket is
	 * being closed here, so the socket object will not be freed either.
	 */
	if (tcpsock_pcb_enqueue(tcp)) {
		assert(!sockevent_is_closing(tcpsock_get_sock(tcp)));

		if (tcpsock_may_close(tcp))
			tcpsock_finish_close(tcp);
		else
			(void)tcpsock_pcb_send(tcp, TRUE /*raise_error*/);
	}
}

/*
 * Shut down a TCP socket for reading and/or writing.
 */
static int
tcpsock_shutdown(struct sock * sock, unsigned int mask)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;

	/*
	 * If the PCB is gone, we want to allow shutdowns for reading but not
	 * writing: shutting down for writing affects the PCB, shutting down
	 * for reading does not.  Also, if the PCB is in CLOSED state, we would
	 * not know how to deal with subsequent operations after a shutdown for
	 * writing, so forbid such calls altogether.
	 */
	if ((tcp->tcp_pcb == NULL || tcp->tcp_pcb->state == CLOSED) &&
	    (mask & SFL_SHUT_WR))
		return ENOTCONN;

	/*
	 * Handle listening sockets as a special case.  Shutting down a
	 * listening socket frees its PCB.  Sockets pending on the accept queue
	 * may still be accepted, but after that, accept(2) will start
	 * returning ECONNABORTED.  This feature allows multi-process server
	 * applications to shut down gracefully, supposedly..
	 */
	if (tcpsock_is_listening(tcp)) {
		if (tcp->tcp_pcb != NULL)
			tcpsock_pcb_close(tcp);

		return OK;
	}

	/*
	 * We control shutdown-for-reading locally, and intentially do not tell
	 * lwIP about it: if we do that and also shut down for writing, the PCB
	 * may disappear (now or eventually), which is not what we want.
	 * Instead, we only tell lwIP to shut down for reading once we actually
	 * want to get rid of the PCB, using tcp_close().  In the meantime, if
	 * the socket is shut down for reading by the user, we simply discard
	 * received data as fast as we can--one out of a number of possible
	 * design choices there, and (reportedly) the one used by the BSDs.
	 */
	if (mask & SFL_SHUT_RD)
		(void)tcpsock_clear_recv(tcp, TRUE /*ack_data*/);

	/*
	 * Shutting down for writing a connecting socket simply closes its PCB.
	 * Closing a PCB in SYN_SENT state simply deallocates it, so this can
	 * not fail.  On the other hand, for connected sockets we want to send
	 * a FIN, which may fail due to memory shortage, in which case we have
	 * to try again later..
	 */
	if (mask & SFL_SHUT_WR) {
		if (tcp->tcp_pcb->state == SYN_SENT)
			tcpsock_pcb_close(tcp);
		else if (!tcpsock_is_shutdown(tcp, SFL_SHUT_WR))
			tcpsock_send_fin(tcp);
	}

	return OK;
}

/*
 * Close a TCP socket.  Complete the operation immediately if possible, or
 * otherwise initiate the closing process and complete it later, notifying
 * libsockevent about that as well.  Depending on linger settings, this
 * function may be called twice on the same socket: the first time with the
 * 'force' flag cleared, and the second time with the 'force' flag set.
 */
static int
tcpsock_close(struct sock * sock, int force)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;
	struct tcpsock *queued;
	size_t rlen;

	assert(tcp->tcp_listener == NULL);

	/*
	 * If this was a listening socket, so abort and clean up any and all
	 * connections on its listener queue.  Note that the listening socket
	 * may or may not have a PCB at this point.
	 */
	if (tcpsock_is_listening(tcp)) {
		while (!TAILQ_EMPTY(&tcp->tcp_queue.tq_head)) {
			queued = TAILQ_FIRST(&tcp->tcp_queue.tq_head);

			tcpsock_pcb_abort(queued);

			(void)tcpsock_cleanup(queued, TRUE /*may_free*/);
		}
	}

	/*
	 * Clear the receive queue, and make sure that we no longer add new
	 * data to it.  The latter is relevant only for the case that we end up
	 * returning SUSPEND below.  Remember whether there were bytes left,
	 * because we should reset the connection if there were.
	 */
	rlen = tcpsock_clear_recv(tcp, FALSE /*ack_data*/);

	sockevent_set_shutdown(tcpsock_get_sock(tcp), SFL_SHUT_RD);

	/*
	 * If the socket is connected, perform a graceful shutdown, unless 1)
	 * we are asked to force-close the socket, or 2) if the local side has
	 * not consumed all data, as per RFC 1122 Sec.4.2.2.13.  Normally lwIP
	 * would take care of the second point, but we may have data in our
	 * receive buffer of which lwIP is not aware.
	 *
	 * Implementing proper linger support is somewhat difficult with lwIP.
	 * In particular, we cannot reliably wait for our FIN to be ACK'ed by
	 * the other side in all cases:
	 *
	 * - the lwIP TCP transition from states CLOSING to TIME_WAIT does not
	 *   trigger any event and once in the TIME_WAIT state, the poll event
	 *   no longer triggers either;
	 * - the lwIP TCP transition from states FIN_WAIT_1 and FIN_WAIT_2 to
	 *   TIME_WAIT will trigger a receive event, but it is not clear
	 *   whether we can reliably check that our FIN was ACK'ed from there.
	 *
	 * That means we have to compromise.  Instead of the proper approach,
	 * we complete our side of the close operation whenever:
	 *
	 * 1. all of or data was acknowledged, AND,
	 * 2. our FIN was sent, AND,
	 * 3a. our FIN was acknowledged, OR,
	 * 3b. we received a FIN from the other side.
	 *
	 * With the addition of the rule 3b, we do not run into the above
	 * reliability problems, but we may return from SO_LINGER-blocked close
	 * calls too early and thus give callers a false impression of success.
	 * TODO: if lwIP ever gets improved on this point, the code in this
	 * module should be rewritten to make use of the improvements.
	 *
	 * The set of rules is basically the same as for closing the PCB early
	 * as per tcpsock_may_close(), except with the check for our FIN being
	 * acknowledged.  Unfortunately only the FIN_WAIT_2, TIME_WAIT, and
	 * (reentered) CLOSED TCP states guarantee that there are no
	 * unacknowledged data segments anymore, so we may have to wait for
	 * reaching any one of these before we can actually finish closing the
	 * socket with tcp_close().
	 *
	 * In addition, lwIP does not tell us when our FIN gets acknowledged,
	 * so we have to use polling and direct access to lwIP's PCB fields
	 * instead, just like lwIP's BSD API does.  There is no other way.
	 * Also, we may not even be able to send the FIN right away, in which
	 * case we must defer that until later.
	 */
	if (tcp->tcp_pcb != NULL) {
		switch (tcp->tcp_pcb->state) {
		case CLOSE_WAIT:
		case CLOSING:
		case LAST_ACK:
			assert(tcpsock_get_flags(tcp) & TCPF_RCVD_FIN);

			/* FALLTHROUGH */
		case SYN_RCVD:
		case ESTABLISHED:
		case FIN_WAIT_1:
			/* First check if we should abort the connection. */
			if (force || rlen > 0)
				break;

			/*
			 * If we have not sent a FIN yet, try sending it now;
			 * if all other conditions are met for closing the
			 * socket, successful FIN transmission will complete
			 * the close.  Otherwise, perform the close check
			 * explicitly.
			 */
			if (!tcpsock_is_shutdown(tcp, SFL_SHUT_WR))
				tcpsock_send_fin(tcp);
			else if (tcpsock_may_close(tcp))
				tcpsock_pcb_close(tcp);

			/*
			 * If at this point the PCB is gone, we managed to
			 * close the connection immediately, and the socket has
			 * already been cleaned up by now.  This may occur if
			 * there is no unacknowledged data and we already
			 * received a FIN earlier on.
			 */
			if (tcp->tcp_pcb == NULL)
				return OK;

			/*
			 * Complete the close operation at a later time.
			 * Adjust the polling interval, so that we can detect
			 * completion of the close as quickly as possible.
			 */
			tcp_poll(tcp->tcp_pcb, tcpsock_event_poll,
			    TCP_POLL_CLOSE_INTERVAL);

			return SUSPEND;

		default:
			/*
			 * The connection is either not yet established, or
			 * already in a state where we can close it right now.
			 */
			tcpsock_pcb_close(tcp);
		}
	}

	/*
	 * Abort the connection is the PCB is still around, and clean up the
	 * socket.  We cannot let tcpsock_cleanup() free the socket object yet,
	 * because we are still in the callback from libsockevent, and the
	 * latter cannot handle the socket object being freed from here.
	 */
	if (tcp->tcp_pcb != NULL)
		tcpsock_pcb_abort(tcp);

	(void)tcpsock_cleanup(tcp, FALSE /*may_free*/);

	return OK;
}

/*
 * Free up a closed TCP socket.
 */
static void
tcpsock_free(struct sock * sock)
{
	struct tcpsock *tcp = (struct tcpsock *)sock;

	assert(tcp->tcp_pcb == NULL);
	assert(tcp->tcp_snd.ts_len == 0);
	assert(tcp->tcp_snd.ts_head == NULL);
	assert(tcp->tcp_rcv.tr_len == 0);
	assert(tcp->tcp_rcv.tr_head == NULL);

	TAILQ_INSERT_HEAD(&tcp_freelist, tcp, tcp_queue.tq_next);
}

/* This table maps TCP states from lwIP numbers to NetBSD numbers. */
static const struct {
	int tsm_tstate;
	int tsm_sostate;
} tcpsock_statemap[] = {
	[CLOSED]	= { TCPS_CLOSED,	SS_ISDISCONNECTED	},
	[LISTEN]	= { TCPS_LISTEN,	0			},
	[SYN_SENT]	= { TCPS_SYN_SENT,	SS_ISCONNECTING		},
	[SYN_RCVD]	= { TCPS_SYN_RECEIVED,	SS_ISCONNECTING		},
	[ESTABLISHED]	= { TCPS_ESTABLISHED,	SS_ISCONNECTED		},
	[FIN_WAIT_1]	= { TCPS_FIN_WAIT_1,	SS_ISDISCONNECTING	},
	[FIN_WAIT_2]	= { TCPS_FIN_WAIT_2,	SS_ISDISCONNECTING	},
	[CLOSE_WAIT]	= { TCPS_CLOSE_WAIT,	SS_ISCONNECTED		},
	[CLOSING]	= { TCPS_CLOSING,	SS_ISDISCONNECTING	},
	[LAST_ACK]	= { TCPS_LAST_ACK,	SS_ISDISCONNECTING	},
	[TIME_WAIT]	= { TCPS_TIME_WAIT,	SS_ISDISCONNECTED	},
};

/*
 * Fill the given kinfo_pcb sysctl(7) structure with information about the TCP
 * PCB identified by the given pointer.
 */
static void
tcpsock_get_info(struct kinfo_pcb * ki, const void * ptr)
{
	const struct tcp_pcb *pcb = (const struct tcp_pcb *)ptr;
	struct tcpsock *tcp;

	/*
	 * Not all TCP PCBs have an associated tcpsock structure.  We are
	 * careful enough clearing the callback argument for PCBs on any of the
	 * TCP lists that we can use that callback argument to determine
	 * whether there is an associated tcpsock structure, although with one
	 * exception: PCBs for incoming connections that have not yet been
	 * fully established (i.e., in SYN_RCVD state).  These will have the
	 * callback argument of the listening socket (which itself may already
	 * have been deallocated at this point) but should not be considered as
	 * associated with the listening socket's tcpsock structure.
	 */
	if (pcb->callback_arg != NULL && pcb->state != SYN_RCVD) {
		tcp = (struct tcpsock *)pcb->callback_arg;
		assert(tcp >= tcp_array &&
		    tcp < &tcp_array[__arraycount(tcp_array)]);

		/* TODO: change this so that sockstat(1) may work one day. */
		ki->ki_sockaddr = (uint64_t)(uintptr_t)tcpsock_get_sock(tcp);
	} else {
		/* No tcpsock.  Could also be in TIME_WAIT state etc. */
		tcp = NULL;

		ki->ki_sostate = SS_NOFDREF;
	}

	ki->ki_type = SOCK_STREAM;

	if ((unsigned int)pcb->state < __arraycount(tcpsock_statemap)) {
		ki->ki_tstate = tcpsock_statemap[pcb->state].tsm_tstate;
		/* TODO: this needs work, but does anything rely on it? */
		ki->ki_sostate |= tcpsock_statemap[pcb->state].tsm_sostate;
	}

	/* Careful with the LISTEN state here (see below). */
	ipsock_get_info(ki, &pcb->local_ip, pcb->local_port,
	    &pcb->remote_ip, (pcb->state != LISTEN) ? pcb->remote_port : 0);

	/*
	 * The PCBs for listening sockets are actually smaller.  Thus, for
	 * listening sockets, do not attempt to access any of the fields beyond
	 * those provided in the smaller structure.
	 */
	if (pcb->state == LISTEN) {
		assert(tcp != NULL);
		ki->ki_refs =
		    (uint64_t)(uintptr_t)TAILQ_FIRST(&tcp->tcp_queue.tq_head);
	} else {
		if (tcp_nagle_disabled(pcb))
			ki->ki_tflags |= NETBSD_TF_NODELAY;

		if (tcp != NULL) {
			ki->ki_rcvq = tcp->tcp_rcv.tr_len;
			ki->ki_sndq = tcp->tcp_snd.ts_len;

			if (tcp->tcp_listener != NULL)
				ki->ki_nextref = (uint64_t)(uintptr_t)
				    TAILQ_NEXT(tcp, tcp_queue.tq_next);
		}
	}
}

/*
 * Given either NULL or a previously returned TCP PCB pointer, return the first
 * or next TCP PCB pointer, or NULL if there are no more.  The current
 * implementation supports only one concurrent iteration at once.
 */
static const void *
tcpsock_enum(const void * last)
{
	static struct {
		unsigned int i;
		const struct tcp_pcb *pcb;
	} iter;

	if (last != NULL && (iter.pcb = iter.pcb->next) != NULL)
		return (const void *)iter.pcb;

	for (iter.i = (last != NULL) ? iter.i + 1 : 0;
	    iter.i < __arraycount(tcp_pcb_lists); iter.i++) {
		if ((iter.pcb = *tcp_pcb_lists[iter.i]) != NULL)
			return (const void *)iter.pcb;
	}

	return NULL;
}

/*
 * Obtain the list of TCP protocol control blocks, for sysctl(7).
 */
static ssize_t
tcpsock_pcblist(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{

	return util_pcblist(call, oldp, tcpsock_enum, tcpsock_get_info);
}

static const struct sockevent_ops tcpsock_ops = {
	.sop_bind		= tcpsock_bind,
	.sop_listen		= tcpsock_listen,
	.sop_connect		= tcpsock_connect,
	.sop_accept		= tcpsock_accept,
	.sop_test_accept	= tcpsock_test_accept,
	.sop_pre_send		= tcpsock_pre_send,
	.sop_send		= tcpsock_send,
	.sop_test_send		= tcpsock_test_send,
	.sop_pre_recv		= tcpsock_pre_recv,
	.sop_recv		= tcpsock_recv,
	.sop_test_recv		= tcpsock_test_recv,
	.sop_ioctl		= ifconf_ioctl,
	.sop_setsockmask	= tcpsock_setsockmask,
	.sop_setsockopt		= tcpsock_setsockopt,
	.sop_getsockopt		= tcpsock_getsockopt,
	.sop_getsockname	= tcpsock_getsockname,
	.sop_getpeername	= tcpsock_getpeername,
	.sop_shutdown		= tcpsock_shutdown,
	.sop_close		= tcpsock_close,
	.sop_free		= tcpsock_free
};
